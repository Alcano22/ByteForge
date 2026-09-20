#include "Engine/Renderer/Renderer.h"
#include "Engine/Renderer/RendererAPI.h"

#include "Platform/Vulkan/VulkanContext.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanFrameData.h"
#include "Platform/Vulkan/VulkanRenderer.h"

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
            VulkanContext::Get().GetRenderer().Submit(*material, *mesh, pushConstants);
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
            VulkanContext::Get().GetRenderer().BeginRenderTarget(*target);
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
