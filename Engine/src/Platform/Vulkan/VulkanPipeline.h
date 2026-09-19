#pragma once

#include "Engine/Core/NonCopyable.h"

#include "Engine/Renderer/Buffer.h"

#include <vulkan/vulkan.h>

namespace ByteForge
{
    class VulkanDevice;
    class VulkanRenderPass;

    class VulkanPipeline : NonCopyable
    {
    public:
        VulkanPipeline(const VulkanDevice& device, const VulkanRenderPass& renderPass,
                       VkShaderModule vertexShader, VkShaderModule fragmentShader,
                       const BufferLayout& vertexLayout, VkDescriptorSetLayout descriptorSetLayout,
                       VkShaderStageFlags pushConstantStageFlags, uint32_t pushConstantSize);
        ~VulkanPipeline();

        [[nodiscard]] VkPipeline GetHandle() const { return m_Pipeline; }
        [[nodiscard]] VkPipelineLayout GetLayoutHandle() const { return m_PipelineLayout; }

    private:
        const VulkanDevice& m_Device;
        VkPipelineLayout m_PipelineLayout = nullptr;
        VkPipeline m_Pipeline = nullptr;
    };
}
