#include "Engine/Renderer/Renderer.h"
#include "Engine/Renderer/RendererAPI.h"

#include "Platform/Vulkan/VulkanContext.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanRenderer.h"
#include "Platform/Vulkan/VulkanVertexBuffer.h"
#include "Platform/Vulkan/VulkanIndexBuffer.h"
#include "Platform/Vulkan/VulkanFrameData.h"
#include "Platform/Vulkan/VulkanPipeline.h"

#include <format>
#include <memory>
#include <stdexcept>

namespace ByteForge
{
    void Renderer::BeginFrame()
    {
        DispatchRHICall([] { VulkanContext::Get().BeginFrame(); });
    }

    void Renderer::BeginScene(const Camera& camera)
    {
        DispatchRHICall([&]
        {
            VulkanContext::Get().GetFrameData().BeginScene({ .ViewProjection = camera.GetViewProjection() });
        });
    }

    void Renderer::SubmitRaw(const Ref<Pipeline>& pipeline, const Ref<Mesh>& mesh,
                             const std::span<const std::byte> pushConstants)
    {
        DispatchRHICall([&]
        {
            const auto& vulkanPipeline = static_cast<const VulkanPipeline&>(*pipeline);
            const auto& vulkanVertexBuffer = static_cast<const VulkanVertexBuffer&>(*mesh->GetVertexBuffer());

            if (pushConstants.size() != vulkanPipeline.GetPushConstantSize())
            {
                throw std::runtime_error(std::format("Renderer::Submit: got {} bytes of push constant data, "
                                                     "but the shader expects {}",
                                                     pushConstants.size(), vulkanPipeline.GetPushConstantSize()));
            }

            VkBuffer indexBufferHandle = nullptr;
            uint32_t indexCount = 0;
            if (mesh->HasIndexBuffer())
            {
                const auto& indexBuffer = static_cast<const VulkanIndexBuffer&>(*mesh->GetIndexBuffer());
                indexBufferHandle = indexBuffer.GetHandle();
                indexCount = indexBuffer.GetCount();
            }

            VulkanContext::Get().GetRenderer().Submit(vulkanPipeline, pushConstants, vulkanVertexBuffer.GetHandle(),
                                                      mesh->GetVertexCount(), indexBufferHandle, indexCount);
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
