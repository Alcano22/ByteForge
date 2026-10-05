#include "Platform/Vulkan/VulkanContext.h"
#include "Platform/Vulkan/VulkanObject.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanFrameData.h"
#include "Platform/Vulkan/VulkanRenderer.h"
#include "Platform/Vulkan/VulkanVertexBuffer.h"
#include "Platform/Vulkan/VulkanIndexBuffer.h"
#include "Platform/Vulkan/VulkanUniformBuffer.h"
#include "Platform/Vulkan/VulkanShaderProgram.h"
#include "Platform/Vulkan/VulkanPipeline.h"
#include "Platform/Vulkan/VulkanMaterial.h"
#include "Platform/Vulkan/VulkanTexture2D.h"
#include "Platform/Vulkan/VulkanRenderTarget.h"
#include "Platform/Vulkan/VulkanImGuiRenderer.h"
#include "Platform/Vulkan/VulkanSwapchain.h"
#include "Platform/Vulkan/VulkanHelpers.h"

namespace ByteForge
{
    Ref<VertexBuffer> VulkanContext::CreateVertexBuffer(const uint32_t size)
    {
        return MakeVulkanObject<VertexBuffer, VulkanVertexBuffer>(size);
    }

    Ref<VertexBuffer> VulkanContext::CreateVertexBuffer(const void* vertices, const uint32_t size)
    {
        return MakeVulkanObject<VertexBuffer, VulkanVertexBuffer>(vertices, size);
    }

    Ref<IndexBuffer> VulkanContext::CreateIndexBuffer(const std::span<const uint32_t> indices)
    {
        return MakeVulkanObject<IndexBuffer, VulkanIndexBuffer>(indices);
    }

    Ref<UniformBuffer> VulkanContext::CreateUniformBuffer(const uint32_t size)
    {
        return MakeVulkanObject<UniformBuffer, VulkanUniformBuffer>(size);
    }

    Ref<Shader> VulkanContext::CreateShader(const std::string& vertexSource, const std::string& fragmentSource)
    {
        return MakeVulkanObject<Shader, VulkanShaderProgram>(vertexSource, fragmentSource);
    }

    Ref<Shader> VulkanContext::CreateShader(const ShaderSource& source)
    {
        return MakeVulkanObject<Shader, VulkanShaderProgram>(source);
    }

    Ref<Pipeline> VulkanContext::CreatePipeline(const PipelineSpec& spec)
    {
        return MakeVulkanObject<Pipeline, VulkanPipeline>(spec);
    }

    Ref<Material> VulkanContext::CreateMaterial(const Ref<Pipeline>& pipeline)
    {
        return MakeVulkanObject<Material, VulkanMaterial>(pipeline);
    }

    Ref<Texture2D> VulkanContext::CreateTexture2D(const uint32_t width, const uint32_t height,
                                                  const std::span<const std::byte> pixels,
                                                  const TextureSettings& settings)
    {
        return MakeVulkanObject<Texture2D, VulkanTexture2D>(width, height, pixels, settings);
    }

    Ref<RenderTarget> VulkanContext::CreateRenderTarget(const RenderTargetSpec& spec)
    {
        return MakeVulkanObject<RenderTarget, VulkanRenderTarget>(spec);
    }

    Scope<ImGuiRenderer> VulkanContext::CreateImGuiRenderer()
    {
        return MakeScope<VulkanImGuiRenderer>();
    }

    void VulkanContext::BeginScene(const CameraUniforms& uniforms)
    {
        m_FrameData->BeginScene(uniforms);
    }

    void VulkanContext::Submit(Material& material, const Mesh& mesh, const std::span<const std::byte> pushConstants,
                               const DrawRange& range)
    {
        m_Renderer->Submit(material, mesh, pushConstants, range);
    }

    void VulkanContext::BeginRenderTarget(const RenderTarget& target)
    {
        m_Renderer->BeginRenderTarget(target);
    }

    void VulkanContext::EndRenderTarget()
    {
        m_Renderer->EndRenderTarget();
    }

    void VulkanContext::WaitIdle()
    {
        m_Device->WaitIdle();
    }

    uint64_t VulkanContext::GetFrameNumber() const
    {
        return m_FrameData->GetFrameNumber();
    }

    bool VulkanContext::IsSwapchainSrgb() const
    {
        return IsSrgbFormat(m_Swapchain->GetImageFormat());
    }
}
