#pragma once

#include "Engine/Core/NonCopyable.h"
#include "Engine/Renderer/Pipeline.h"

#include <vulkan/vulkan.h>

namespace ByteForge
{
    class VulkanDevice;
    class VulkanShaderProgram;
    struct ShaderReflection;
    struct ReflectedPushConstants;

    class VulkanPipeline : public Pipeline, NonCopyable
    {
    public:
        explicit VulkanPipeline(const PipelineSpec& spec);
        ~VulkanPipeline() override;

        [[nodiscard]] VkPipeline GetHandle() const { return m_Pipeline; }
        [[nodiscard]] VkPipelineLayout GetLayoutHandle() const { return m_PipelineLayout; }
        [[nodiscard]] VkShaderStageFlags GetPushConstantStages() const { return m_PushConstantStages; }
        [[nodiscard]] uint32_t GetPushConstantSize() const { return m_PushConstantSize; }

    private:
        void CreatePipelineLayout(const ReflectedPushConstants& pushConstants);
        void CreatePipeline(const PipelineSpec& spec, const VulkanShaderProgram& shader);

    private:
        const VulkanDevice& m_Device;

        VkPipelineLayout m_PipelineLayout = nullptr;
        VkPipeline m_Pipeline = nullptr;
        VkShaderStageFlags m_PushConstantStages = 0;
        uint32_t m_PushConstantSize = 0;
    };
}
