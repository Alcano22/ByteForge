#pragma once

#include "Engine/Core/NonCopyable.h"
#include "Engine/Renderer/Material.h"
#include "Platform/Vulkan/VulkanShaderReflection.h"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <limits>
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

        void Flush();

        [[nodiscard]] const VulkanPipeline& GetVulkanPipeline() const;

        [[nodiscard]] VkDescriptorSet GetDescriptorSet(uint32_t frameIndex) const;

    private:
        Ref<Pipeline> m_Pipeline;

        std::vector<ReflectedBlockMember> m_Members;
        std::vector<std::byte> m_Shadow;
        std::vector<bool> m_FrameDirty;
        Scope<VulkanUniformBuffer> m_Buffer;
        std::vector<VkDescriptorSet> m_Sets;

        uint64_t m_LastDrawFrame = std::numeric_limits<uint64_t>::max();
    };
}
