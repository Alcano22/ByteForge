#pragma once

#include "Engine/Core/Core.h"
#include "Engine/ImGui/ImGuiRenderer.h"
#include "Engine/Renderer/Buffer.h"
#include "Engine/Renderer/CameraUniforms.h"
#include "Engine/Renderer/DrawRange.h"
#include "Engine/Renderer/GraphicsContext.h"
#include "Engine/Renderer/Material.h"
#include "Engine/Renderer/Mesh.h"
#include "Engine/Renderer/Pipeline.h"
#include "Engine/Renderer/RenderTarget.h"
#include "Engine/Renderer/Shader.h"
#include "Engine/Renderer/Texture2D.h"
#include "Engine/Renderer/UniformBuffer.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace ByteForge
{
    class RenderBackend : public GraphicsContext
    {
    public:
        [[nodiscard]] virtual Ref<VertexBuffer> CreateVertexBuffer(uint32_t size) = 0;
        [[nodiscard]] virtual Ref<VertexBuffer> CreateVertexBuffer(const void* vertices, uint32_t size) = 0;

        [[nodiscard]] virtual Ref<IndexBuffer> CreateIndexBuffer(std::span<const uint32_t> indices) = 0;

        [[nodiscard]] virtual Ref<UniformBuffer> CreateUniformBuffer(uint32_t size) = 0;

        [[nodiscard]] virtual Ref<Shader> CreateShader(const std::string& vertexSource,
                                                       const std::string& fragmentSource) = 0;

        [[nodiscard]] virtual Ref<Pipeline> CreatePipeline(const PipelineSpec& spec) = 0;

        [[nodiscard]] virtual Ref<Material> CreateMaterial(const Ref<Pipeline>& pipeline) = 0;

        [[nodiscard]] virtual Ref<Texture2D> CreateTexture2D(uint32_t width, uint32_t height,
                                                             std::span<const std::byte> pixels,
                                                             const TextureSettings& settings) = 0;

        [[nodiscard]] virtual Ref<RenderTarget> CreateRenderTarget(const RenderTargetSpec& spec) = 0;

        [[nodiscard]] virtual Scope<ImGuiRenderer> CreateImGuiRenderer() = 0;

        virtual void BeginFrame() = 0;
        virtual void EndFrame() = 0;
        virtual void BeginScene(const CameraUniforms& uniforms) = 0;
        virtual void Submit(Material& material, const Mesh& mesh, std::span<const std::byte> pushConstants,
                            const DrawRange& range) = 0;
        virtual void BeginRenderTarget(const RenderTarget& target) = 0;
        virtual void EndRenderTarget() = 0;
        virtual void OnFramebufferResized() = 0;
        virtual void WaitIdle() = 0;
        [[nodiscard]] virtual uint64_t GetFrameNumber() const = 0;
        [[nodiscard]] virtual bool IsSwapchainSrgb() const = 0;

        [[nodiscard]] static RenderBackend& Get();
        [[nodiscard]] static RenderBackend* TryGet() { return s_Active; }

    protected:
        static void SetActive(RenderBackend* backend) { s_Active = backend; }

    private:
        static RenderBackend* s_Active;
    };
}
