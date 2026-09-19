#include "Platform/Vulkan/VulkanShaderProgram.h"
#include "Platform/Vulkan/VulkanShader.h"
#include "Platform/Vulkan/VulkanPipeline.h"
#include "Platform/Vulkan/VulkanDescriptorSetLayout.h"
#include "Platform/Vulkan/VulkanDescriptorPool.h"
#include "Platform/Vulkan/VulkanUniformBuffer.h"
#include "Platform/Vulkan/VulkanContext.h"

namespace ByteForge
{
    namespace
    {
        VkShaderStageFlags ToVkShaderStageFlags(const uint32_t flags)
        {
            VkShaderStageFlags result = 0;
            if (flags & ShaderStageVertex)   result |= VK_SHADER_STAGE_VERTEX_BIT;
            if (flags & ShaderStageFragment) result |= VK_SHADER_STAGE_FRAGMENT_BIT;
            return result;
        }
    }

    VulkanShaderProgram::VulkanShaderProgram(const std::string& vertexSrc, const std::string& fragmentSrc,
                                             const BufferLayout& vertexLayout, const uint32_t uniformBufferSize,
                                             const uint32_t uniformStageFlags, const uint32_t pushConstantStageFlags,
                                             const uint32_t pushConstantSize)
    {
        auto& device = VulkanContext::Get().GetDevice();
        auto& renderPass = VulkanContext::Get().GetRenderPass();

        m_DescriptorSetLayout = MakeScope<VulkanDescriptorSetLayout>(device, ToVkShaderStageFlags(uniformStageFlags));
        m_UniformBuffer = MakeScope<VulkanUniformBuffer>(uniformBufferSize);
        m_DescriptorPool = MakeScope<VulkanDescriptorPool>(device, m_DescriptorSetLayout->GetHandle(),
                                                           *m_UniformBuffer, VulkanContext::GetFramesInFlight());

        const VulkanShader vertexShader(device, vertexSrc, ShaderStage::Vertex);
        const VulkanShader fragmentShader(device, fragmentSrc, ShaderStage::Fragment);

        m_Pipeline = MakeScope<VulkanPipeline>(device, renderPass, vertexShader.GetHandle(),
                                               fragmentShader.GetHandle(), vertexLayout,
                                               m_DescriptorSetLayout->GetHandle(),
                                               ToVkShaderStageFlags(pushConstantStageFlags),
                                               pushConstantSize);
    }

    VulkanShaderProgram::~VulkanShaderProgram() = default;

    void VulkanShaderProgram::SetUniformData(const void* data, const uint32_t size)
    {
        m_UniformBuffer->SetData(data, size);
    }

    VkPipeline VulkanShaderProgram::GetPipelineHandle() const { return m_Pipeline->GetHandle(); }
    VkPipelineLayout VulkanShaderProgram::GetPipelineLayoutHandle() const { return m_Pipeline->GetLayoutHandle(); }

    VkDescriptorSet VulkanShaderProgram::GetDescriptorSet(const uint32_t frameIndex) const
    {
        return m_DescriptorPool->GetSet(frameIndex);
    }
}
