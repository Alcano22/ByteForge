#pragma once

#include "Engine/Core/NonCopyable.h"
#include "Engine/Renderer/Pipeline.h"
#include "Platform/Vulkan/VulkanShaderReflection.h"

#include <vulkan/vulkan.h>

#include <vector>

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
        [[nodiscard]] const std::vector<ReflectedDescriptorBinding>& GetMaterialBindings() const
        {
            return m_MaterialBindings;
        }

        [[nodiscard]] VkShaderStageFlags GetPushConstantStages() const { return m_PushConstantStages; }
        [[nodiscard]] uint32_t GetPushConstantSize() const { return m_PushConstantSize; }
        [[nodiscard]] const std::vector<VkFormat>& GetColorFormats() const { return m_ColorFormats; }
        [[nodiscard]] VkFormat GetDepthFormat() const { return m_DepthFormat; }

    private:
        void CreateMaterialSetLayout(const ShaderReflection& reflection);
        void CreatePipelineLayout(const ReflectedPushConstants& pushConstants);
        void CreatePipeline(const PipelineSpec& spec, const VulkanShaderProgram& shader);

    private:
        const VulkanDevice& m_Device;

        Scope<VulkanDescriptorSetLayout> m_MaterialSetLayout;
        std::vector<ReflectedDescriptorBinding> m_MaterialBindings;

        VkPipelineLayout m_PipelineLayout = nullptr;
        VkPipeline m_Pipeline = nullptr;
        VkShaderStageFlags m_PushConstantStages = 0;
        uint32_t m_PushConstantSize = 0;
        std::vector<VkFormat> m_ColorFormats;
        VkFormat m_DepthFormat = VK_FORMAT_UNDEFINED;
    };
}
