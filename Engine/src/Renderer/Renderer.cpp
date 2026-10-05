#include "Engine/Renderer/Renderer.h"
#include "RenderBackend.h"

#include <stdexcept>

namespace ByteForge
{
    void Renderer::BeginFrame() { RenderBackend::Get().BeginFrame(); }
    void Renderer::EndFrame() { RenderBackend::Get().EndFrame(); }

    void Renderer::BeginScene(const Camera& camera)
    {
        RenderBackend::Get().BeginScene({ .ViewProjection = camera.GetViewProjection() });
    }

    void Renderer::SubmitRaw(const Ref<Material>& material, const Ref<Mesh>& mesh,
                             const std::span<const std::byte> pushConstants, const DrawRange& range)
    {
        if (!material || !mesh)
            throw std::runtime_error("Renderer::Submit: material and mesh must not be null");

        RenderBackend::Get().Submit(*material, *mesh, pushConstants, range);
    }

    void Renderer::BeginRenderTarget(const Ref<RenderTarget>& target)
    {
        if (!target)
            throw std::runtime_error("Renderer::BeginRenderTarget: the render target must not be null");

        RenderBackend::Get().BeginRenderTarget(*target);
    }

    void Renderer::EndRenderTarget() { RenderBackend::Get().EndRenderTarget(); }
    void Renderer::OnFramebufferResized() { RenderBackend::Get().OnFramebufferResized(); }
    void Renderer::WaitIdle() { RenderBackend::Get().WaitIdle(); }
    uint64_t Renderer::GetFrameNumber() { return RenderBackend::Get().GetFrameNumber(); }

    bool Renderer::IsSrgb(const ImageFormat format)
    {
        if (format == ImageFormat::Swapchain)
            return RenderBackend::Get().IsSwapchainSrgb();

        return format == ImageFormat::RGBA8_SRGB;
    }
}
