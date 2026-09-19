#include "Engine/Renderer/Renderer.h"
#include "Engine/Renderer/RendererAPI.h"
#include "Engine/Core/Log.h"

#include "Platform/Vulkan/VulkanContext.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanRenderer.h"
#include "Platform/Vulkan/VulkanShaderProgram.h"
#include "Platform/Vulkan/VulkanVertexBuffer.h"
#include "Platform/Vulkan/VulkanIndexBuffer.h"

#include <memory>

namespace ByteForge
{
    void Renderer::BeginFrame()
    {
        DispatchRHICall([] { VulkanContext::Get().BeginFrame(); });
    }

    void Renderer::Submit(const Ref<Shader>& shader, const Ref<Mesh>& mesh, const glm::mat4& transform)
    {
        DispatchRHICall([&]
        {
            const auto vulkanShader = std::static_pointer_cast<VulkanShaderProgram>(shader);
            const auto vulkanVertexBuffer = std::static_pointer_cast<VulkanVertexBuffer>(mesh->GetVertexBuffer());

            const VkBuffer indexBuffer = mesh->HasIndexBuffer()
                ? std::static_pointer_cast<VulkanIndexBuffer>(mesh->GetIndexBuffer())->GetHandle() : nullptr;
            const uint32_t indexCount = mesh->HasIndexBuffer()
                ? std::static_pointer_cast<VulkanIndexBuffer>(mesh->GetIndexBuffer())->GetCount() : 0;

            const auto& renderer = VulkanContext::Get().GetRenderer();
            const VkDescriptorSet descriptorSet = vulkanShader->GetDescriptorSet(renderer.GetCurrentFrameIndex());
            renderer.Submit(vulkanShader->GetPipelineHandle(), vulkanShader->GetPipelineLayoutHandle(), descriptorSet,
                transform, vulkanVertexBuffer->GetHandle(), mesh->GetVertexCount(), indexBuffer, indexCount);
        });
    }

    void Renderer::EndFrame()
    {
        DispatchRHICall([] { VulkanContext::Get().EndFrame(); });
    }

    void Renderer::OnWindowResized()
    {
        DispatchRHICall([] { VulkanContext::Get().NotifyFramebufferResized(); });
    }

    void Renderer::WaitIdle()
    {
        DispatchRHICall([] { VulkanContext::Get().GetDevice().WaitIdle(); });
    }
}
