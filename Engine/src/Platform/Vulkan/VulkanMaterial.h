#pragma once

#include "Engine/Core/NonCopyable.h"
#include "Engine/Renderer/Material.h"
#include "Platform/Vulkan/VulkanShaderReflection.h"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace ByteForge
{
    class VulkanPipeline;
    class VulkanUniformBuffer;

    class VulkanMaterial : public Material, NonCopyable
    {
    public:
        explicit VulkanMaterial(const Ref<Pipeline>& pipeline);
        ~VulkanMaterial() override;

        [[nodiscard]] const Ref<Pipeline>& GetPipeline() const override { return m_Pipeline; }

        void SetRaw(std::string_view name, std::span<const std::byte> data) override;
        void SetTexture(std::string_view name, const Ref<Texture2D>& texture) override;

        void Flush();

        [[nodiscard]] const VulkanPipeline& GetVulkanPipeline() const;

        [[nodiscard]] VkDescriptorSet GetDescriptorSet(u32 frameIndex) const;

    private:
        [[nodiscard]] bool WasDrawnThisFrame() const;
        void WriteTextureDescriptors(u32 frame) const;

    private:
        static constexpr u32 NoBinding = std::numeric_limits<u32>::max();

        struct TextureSlot
        {
            std::string Name;
            u32 ImageBinding = 0;
            u32 SamplerBinding = NoBinding;
            Ref<Texture2D> Texture;
        };

        Ref<Pipeline> m_Pipeline;

        std::vector<ReflectedBlockMember> m_Members;
        std::vector<std::byte> m_Shadow;
        std::vector<TextureSlot> m_TextureSlots;
        std::vector<bool> m_FrameDirty;
        Scope<VulkanUniformBuffer> m_Buffer;
        std::vector<VkDescriptorSet> m_Sets;

        u64 m_LastDrawFrame = std::numeric_limits<u64>::max();
    };
}
