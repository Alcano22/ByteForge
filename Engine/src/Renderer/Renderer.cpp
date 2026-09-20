#include "Engine/Renderer/Renderer.h"
#include "Engine/Renderer/RendererAPI.h"

#include "Platform/Vulkan/VulkanContext.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanRenderer.h"
#include "Platform/Vulkan/VulkanVertexBuffer.h"
#include "Platform/Vulkan/VulkanIndexBuffer.h"
#include "Platform/Vulkan/VulkanFrameData.h"
#include "Platform/Vulkan/VulkanPipeline.h"
#include "Platform/Vulkan/VulkanMaterial.h"
#include "Platform/Vulkan/VulkanRenderTarget.h"

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

    void Renderer::SubmitRaw(const Ref<Material>& material, const Ref<Mesh>& mesh,
                             const std::span<const std::byte> pushConstants)
    {
        if (!material || !mesh)
            throw std::runtime_error("Renderer::Submit: material and mesh must not be null");

        DispatchRHICall([&]
        {
            auto& vulkanMaterial = static_cast<VulkanMaterial&>(*material);
            const VulkanPipeline& vulkanPipeline = vulkanMaterial.GetVulkanPipeline();
            const auto& vulkanVertexBuffer = static_cast<const VulkanVertexBuffer&>(*mesh->GetVertexBuffer());

            if (pushConstants.size() != vulkanPipeline.GetPushConstantSize())
            {
                throw std::runtime_error(std::format("Renderer::Submit: got {} bytes of push constant data, "
                                                     "but the shader expects {}",
                                                     pushConstants.size(), vulkanPipeline.GetPushConstantSize()));
            }

            const VkBuffer vertexBufferHandle = vulkanVertexBuffer.GetHandleForDraw();

            VkBuffer indexBufferHandle = nullptr;
            uint32_t indexCount = 0;
            if (mesh->HasIndexBuffer())
            {
                const auto& indexBuffer = static_cast<const VulkanIndexBuffer&>(*mesh->GetIndexBuffer());
                indexBufferHandle = indexBuffer.GetHandle();
                indexCount = indexBuffer.GetCount();
            }

            vulkanMaterial.Flush();

            VulkanContext::Get().GetRenderer().Submit(vulkanMaterial, pushConstants, vertexBufferHandle,
                                                      mesh->GetVertexCount(), indexBufferHandle, indexCount);
        });
    }

    void Renderer::EndFrame()
    {
        DispatchRHICall([] { VulkanContext::Get().EndFrame(); });
    }

    void Renderer::BeginRenderTarget(const Ref<RenderTarget>& target)
    {
        if (!target)
            throw std::runtime_error("Renderer::BeginRenderTarget: the render target must not be null");

        DispatchRHICall([&]
        {
            VulkanContext::Get().GetRenderer().BeginRenderTarget(static_cast<const VulkanRenderTarget&>(*target));
        });
    }

    void Renderer::EndRenderTarget()
    {
        DispatchRHICall([] { VulkanContext::Get().GetRenderer().EndRenderTarget(); });
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
