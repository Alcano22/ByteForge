#include "Platform/Vulkan/VulkanMaterial.h"
#include "Platform/Vulkan/VulkanContext.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanDescriptorAllocator.h"
#include "Platform/Vulkan/VulkanFrameData.h"
#include "Platform/Vulkan/VulkanPipeline.h"
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
        const ReflectedDescriptorBinding* binding = vulkanPipeline.GetMaterialBinding();
        if (binding == nullptr) return;

        constexpr uint32_t framesInFlight = VulkanContext::GetFramesInFlight();

        m_Members = binding->Members;
        m_Shadow.assign(binding->Size, std::byte{0});
        m_FrameDirty.assign(framesInFlight, true);
        m_Buffer = MakeScope<VulkanUniformBuffer>(binding->Size);
        m_Sets = VulkanContext::Get().GetDescriptorAllocator().Allocate(vulkanPipeline.GetMaterialSetLayout(),
                                                                        framesInFlight);

        for (uint32_t frame = 0; frame < framesInFlight; ++frame)
        {
            const VkDescriptorBufferInfo bufferInfo{
                .buffer = m_Buffer->GetHandle(frame),
                .offset = 0,
                .range  = binding->Size
            };

            const VkWriteDescriptorSet write{
                .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet          = m_Sets[frame],
                .dstBinding      = binding->Binding,
                .descriptorCount = 1,
                .descriptorType  = binding->Type,
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
                "Material parameter '{}' changed after the material was drawn in the same frame", name
            ));
        }

        std::memcpy(destination, data.data(), data.size());
        std::ranges::fill(m_FrameDirty, true);
    }

    void VulkanMaterial::Flush()
    {
        m_LastDrawFrame = VulkanContext::Get().GetFrameData().GetFrameNumber();

        if (!m_Buffer) return;

        const uint32_t frame = VulkanContext::Get().GetCurrentFrameIndex();
        if (m_FrameDirty[frame])
        {
            m_Buffer->SetData(m_Shadow.data(), static_cast<uint32_t>(m_Shadow.size()));
            m_FrameDirty[frame] = false;
        }
    }

    const VulkanPipeline& VulkanMaterial::GetVulkanPipeline() const
    {
        return static_cast<const VulkanPipeline&>(*m_Pipeline);
    }

    VkDescriptorSet VulkanMaterial::GetDescriptorSet(const uint32_t frameIndex) const
    {
        return m_Sets.empty() ? nullptr : m_Sets[frameIndex];
    }
}
