#pragma once

#include "Engine/Core/NonCopyable.h"
#include "Engine/Renderer/Pipeline.h"
#include "Platform/Vulkan/VulkanShaderReflection.h"

#include <vulkan/vulkan.h>

#include <optional>

namespace ByteForge
{
    class VulkanDevice;
    class VulkanShaderProgram;
    class VulkanDescriptorSetLayout;
    struct ReflectedPushConstants;

    class VulkanPipeline : public Pipeline, NonCopyable
    {
    public:
        explicit VulkanPipeline(const PipelineSpec& spec);
        ~VulkanPipeline() override;

        [[nodiscard]] VkPipeline GetHandle() const { return m_Pipeline; }
        [[nodiscard]] VkPipelineLayout GetLayoutHandle() const { return m_PipelineLayout; }

        [[nodiscard]] VkDescriptorSetLayout GetMaterialSetLayout() const;
        [[nodiscard]] const ReflectedDescriptorBinding* GetMaterialBinding() const
        {
            return m_MaterialBinding ? &*m_MaterialBinding : nullptr;
        }

        [[nodiscard]] VkShaderStageFlags GetPushConstantStages() const { return m_PushConstantStages; }
        [[nodiscard]] uint32_t GetPushConstantSize() const { return m_PushConstantSize; }
        [[nodiscard]] VkFormat GetColorFormat() const { return m_ColorFormat; }
        [[nodiscard]] VkFormat GetDepthFormat() const { return m_DepthFormat; }

    private:
        void CreateMaterialSetLayout(const ShaderReflection& reflection);
        void CreatePipelineLayout(const ReflectedPushConstants& pushConstants);
        void CreatePipeline(const PipelineSpec& spec, const VulkanShaderProgram& shader);

    private:
        const VulkanDevice& m_Device;

        Scope<VulkanDescriptorSetLayout> m_MaterialSetLayout;
        std::optional<ReflectedDescriptorBinding> m_MaterialBinding;

        VkPipelineLayout m_PipelineLayout = nullptr;
        VkPipeline m_Pipeline = nullptr;
        VkShaderStageFlags m_PushConstantStages = 0;
        uint32_t m_PushConstantSize = 0;
        VkFormat m_ColorFormat = VK_FORMAT_UNDEFINED;
        VkFormat m_DepthFormat = VK_FORMAT_UNDEFINED;
    };
}
