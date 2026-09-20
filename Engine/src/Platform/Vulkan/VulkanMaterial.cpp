#include "Platform/Vulkan/VulkanMaterial.h"
#include "Platform/Vulkan/VulkanContext.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanDescriptorAllocator.h"
#include "Platform/Vulkan/VulkanFrameData.h"
#include "Platform/Vulkan/VulkanPipeline.h"
#include "Platform/Vulkan/VulkanTexture2D.h"
#include "Platform/Vulkan/VulkanUniformBuffer.h"

#include <algorithm>
#include <cstring>
#include <format>
#include <stdexcept>

namespace ByteForge
{
    VulkanMaterial::VulkanMaterial(const Ref<Pipeline>& pipeline)
        : m_Pipeline(pipeline)
    {
        if (!m_Pipeline)
            throw std::runtime_error("Material::Create: the pipeline must not be null");

        const VulkanPipeline& vulkanPipeline = GetVulkanPipeline();
        const std::vector<ReflectedDescriptorBinding>& bindings = vulkanPipeline.GetMaterialBindings();
        if (bindings.empty()) return;

        constexpr uint32_t framesInFlight = VulkanContext::GetFramesInFlight();

        const ReflectedDescriptorBinding* parameterBinding = nullptr;

        for (const ReflectedDescriptorBinding& binding : bindings)
        {
            switch (binding.Type)
            {
                case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                    parameterBinding = &binding;
                    m_Members = binding.Members;
                    m_Shadow.assign(binding.Size, std::byte{ 0 });
                    m_Buffer = MakeScope<VulkanUniformBuffer>(binding.Size);
                    break;

                case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
                    m_TextureSlots.push_back({ .Name = binding.Name, .ImageBinding = binding.Binding });
                    break;

                default: break;
            }
        }

        for (const ReflectedDescriptorBinding& binding : bindings)
        {
            if (binding.Type != VK_DESCRIPTOR_TYPE_SAMPLER) continue;

            const auto slot = std::ranges::find(m_TextureSlots, TextureNameOfSampler(binding.Name),
                                                &TextureSlot::Name);
            if (slot != m_TextureSlots.end())
                slot->SamplerBinding = binding.Binding;
        }

        m_FrameDirty.assign(framesInFlight, true);
        m_Sets = VulkanContext::Get().GetDescriptorAllocator().Allocate(vulkanPipeline.GetMaterialSetLayout(),
                                                                        framesInFlight);

        if (parameterBinding == nullptr) return;

        for (uint32_t frame = 0; frame < framesInFlight; ++frame)
        {
            const VkDescriptorBufferInfo bufferInfo{
                .buffer = m_Buffer->GetHandle(frame),
                .offset = 0,
                .range  = parameterBinding->Size
            };

            const VkWriteDescriptorSet write{
                .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet          = m_Sets[frame],
                .dstBinding      = parameterBinding->Binding,
                .descriptorCount = 1,
                .descriptorType  = parameterBinding->Type,
                .pBufferInfo     = &bufferInfo
            };

            vkUpdateDescriptorSets(VulkanContext::Get().GetDevice().GetHandle(), 1, &write, 0, nullptr);
        }
    }

    VulkanMaterial::~VulkanMaterial()
    {
        if (!m_Sets.empty())
            VulkanContext::Get().GetDescriptorAllocator().Free(m_Sets);
    }

    void VulkanMaterial::SetRaw(const std::string_view name, const std::span<const std::byte> data)
    {
        const auto member = std::ranges::find(m_Members, name, &ReflectedBlockMember::Name);
        if (member == m_Members.end())
            throw std::runtime_error(std::format("Material has no parameter '{}'", name));

        if (data.size() != member->Size)
        {
            throw std::runtime_error(std::format("Material parameter '{}' is {} bytes, but {} bytes were given",
                                                 name, member->Size, data.size()));
        }

        std::byte* destination = m_Shadow.data() + member->Offset;
        if (std::memcmp(destination, data.data(), data.size()) == 0) return;

        if (m_LastDrawFrame == VulkanContext::Get().GetFrameData().GetFrameNumber())
        {
            throw std::runtime_error(std::format(
                "Material parameter '{}' changed after the material was drawn in the same frame", name));
        }

        std::memcpy(destination, data.data(), data.size());
        std::ranges::fill(m_FrameDirty, true);
    }

    void VulkanMaterial::SetTexture(const std::string_view name, const Ref<Texture2D>& texture)
    {
        const auto slot = std::ranges::find(m_TextureSlots, name, &TextureSlot::Name);
        if (slot == m_TextureSlots.end())
        {
            std::string available;
            for (const TextureSlot& other : m_TextureSlots)
            {
                if (!available.empty())
                    available += ", ";
                available += other.Name;
            }

            throw std::runtime_error(std::format(
                "Material has no texture '{}' (textures of this material: {}); note that the shader compiler "
                "removes resources the shader does not use", name, available.empty() ? "none" : available));
        }

        if (!texture)
            throw std::runtime_error(std::format("Material texture '{}' must not be null", name));

        if (slot->Texture == texture) return;

        if (WasDrawnThisFrame())
        {
            throw std::runtime_error(std::format(
                "Material texture '{}' changed after the material was drawn in the same frame", name));
        }

        slot->Texture = texture;
        std::ranges::fill(m_FrameDirty, true);
    }

    void VulkanMaterial::Flush()
    {
        for (const TextureSlot& slot : m_TextureSlots)
        {
            if (!slot.Texture)
                throw std::runtime_error(std::format("Material texture '{}' was not set before drawing", slot.Name));
        }

        m_LastDrawFrame = VulkanContext::Get().GetFrameData().GetFrameNumber();

        if (m_Sets.empty()) return;

        const uint32_t frame = VulkanContext::Get().GetCurrentFrameIndex();
        if (!m_FrameDirty[frame]) return;

        if (m_Buffer)
            m_Buffer->SetData(m_Shadow.data(), static_cast<uint32_t>(m_Shadow.size()));

        WriteTextureDescriptors(frame);

        m_FrameDirty[frame] = false;
    }

    const VulkanPipeline& VulkanMaterial::GetVulkanPipeline() const
    {
        return static_cast<const VulkanPipeline&>(*m_Pipeline);
    }

    VkDescriptorSet VulkanMaterial::GetDescriptorSet(const uint32_t frameIndex) const
    {
        return m_Sets.empty() ? nullptr : m_Sets[frameIndex];
    }

    bool VulkanMaterial::WasDrawnThisFrame() const
    {
        const VulkanContext& context = VulkanContext::Get();
        return context.IsInFrame() && m_LastDrawFrame == context.GetFrameData().GetFrameNumber();
    }

    void VulkanMaterial::WriteTextureDescriptors(const uint32_t frame) const
    {
        if (m_TextureSlots.empty()) return;

        std::vector<VkDescriptorImageInfo> imageInfos;
        imageInfos.reserve(m_TextureSlots.size() * 2);

        std::vector<VkWriteDescriptorSet> writes;
        writes.reserve(m_TextureSlots.size() * 2);

        for (const TextureSlot& slot : m_TextureSlots)
        {
            const auto& texture = static_cast<const VulkanTexture2D&>(*slot.Texture);

            imageInfos.push_back({
                .imageView   = texture.GetView(),
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            });
            writes.push_back({
                .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet          = m_Sets[frame],
                .dstBinding      = slot.ImageBinding,
                .descriptorCount = 1,
                .descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                .pImageInfo      = &imageInfos.back()
            });

            if (slot.SamplerBinding == NoBinding) continue;

            imageInfos.push_back({ .sampler = texture.GetSampler() });
            writes.push_back({
                .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet          = m_Sets[frame],
                .dstBinding      = slot.SamplerBinding,
                .descriptorCount = 1,
                .descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLER,
                .pImageInfo      = &imageInfos.back()
            });
        }

        vkUpdateDescriptorSets(VulkanContext::Get().GetDevice().GetHandle(),
                               static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
}
