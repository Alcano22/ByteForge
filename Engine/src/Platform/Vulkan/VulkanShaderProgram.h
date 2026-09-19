#pragma once

#include "Engine/Renderer/Shader.h"
#include "Engine/Core/Core.h"

#include <vulkan/vulkan.h>

#include <string>

namespace ByteForge
{
    class VulkanPipeline;
    class VulkanDescriptorSetLayout;
    class VulkanDescriptorPool;
    class VulkanUniformBuffer;

    class VulkanShaderProgram : public Shader
    {
    public:
        VulkanShaderProgram(const std::string& vertexSrc, const std::string& fragmentSrc,
                            const BufferLayout& vertexLayout, uint32_t uniformBufferSize,
                            uint32_t uniformStageFlags, uint32_t pushConstantStageFlags,
                            uint32_t pushConstantSize);
        ~VulkanShaderProgram() override;

        void SetUniformData(const void* data, uint32_t size) override;

        [[nodiscard]] VkPipeline GetPipelineHandle() const;
        [[nodiscard]] VkPipelineLayout GetPipelineLayoutHandle() const;
        [[nodiscard]] VkDescriptorSet GetDescriptorSet(uint32_t frameIndex) const;

    private:
        Scope<VulkanDescriptorSetLayout> m_DescriptorSetLayout;
        Scope<VulkanPipeline> m_Pipeline;
        Scope<VulkanUniformBuffer> m_UniformBuffer;
        Scope<VulkanDescriptorPool> m_DescriptorPool;
    };
}
