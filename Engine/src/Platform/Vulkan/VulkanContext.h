#pragma once

#include "../../Renderer/RenderBackend.h"

#include <vulkan/vulkan.h>

#include <memory>

#include "VulkanInstance.h"

struct GLFWwindow;

namespace ByteForge
{
    class VulkanInstance;
    class VulkanDevice;
    class VulkanAllocator;
    class VulkanUploader;
    class VulkanDescriptorAllocator;
    class VulkanSwapchain;
    class VulkanCommandPool;
    class VulkanSyncObjects;
    class VulkanFrameData;
    class VulkanRenderer;
    class VulkanDeletionQueue;
    class VulkanSamplerCache;

    class VulkanContext : public RenderBackend
    {
    public:
        explicit VulkanContext(GLFWwindow* windowHandle);
        ~VulkanContext() override;

        void Init() override;

        [[nodiscard]] Ref<VertexBuffer> CreateVertexBuffer(uint32_t size) override;
        [[nodiscard]] Ref<VertexBuffer> CreateVertexBuffer(const void* vertices, uint32_t size) override;

        [[nodiscard]] Ref<IndexBuffer> CreateIndexBuffer(std::span<const uint32_t> indices) override;

        [[nodiscard]] Ref<UniformBuffer> CreateUniformBuffer(uint32_t size) override;

        [[nodiscard]] Ref<Shader> CreateShader(const std::string& vertexSource,
                                               const std::string& fragmentSource) override;

        [[nodiscard]] Ref<Pipeline> CreatePipeline(const PipelineSpec& spec) override;

        [[nodiscard]] Ref<Material> CreateMaterial(const Ref<Pipeline>& pipeline) override;

        [[nodiscard]] Ref<Texture2D> CreateTexture2D(uint32_t width, uint32_t height,
                                                     std::span<const std::byte> pixels,
                                                     const TextureSettings& settings) override;

        [[nodiscard]] Ref<RenderTarget> CreateRenderTarget(const RenderTargetSpec& spec) override;

        [[nodiscard]] Scope<ImGuiRenderer> CreateImGuiRenderer() override;

        void BeginFrame() override;
        void EndFrame() override;
        void BeginScene(const CameraUniforms& uniforms) override;
        void Submit(Material& material, const Mesh& mesh, std::span<const std::byte> pushConstants,
                    const DrawRange& range) override;
        void BeginRenderTarget(const RenderTarget& target) override;
        void EndRenderTarget() override;
        void OnFramebufferResized() override { m_FramebufferResized = true; }
        void WaitIdle() override;
        [[nodiscard]] uint64_t GetFrameNumber() const override;
        [[nodiscard]] bool IsSwapchainSrgb() const override;

        [[nodiscard]] VkInstance GetInstanceHandle() const { return m_Instance->GetHandle(); }
        [[nodiscard]] VulkanDevice& GetDevice() const { return *m_Device; }
        [[nodiscard]] VulkanAllocator& GetAllocator() const { return *m_Allocator; }
        [[nodiscard]] VulkanUploader& GetUploader() const { return *m_Uploader; }
        [[nodiscard]] VulkanDescriptorAllocator& GetDescriptorAllocator() const { return *m_DescriptorAllocator; }
        [[nodiscard]] VulkanSwapchain& GetSwapchain() const { return *m_Swapchain; }
        [[nodiscard]] VulkanRenderer& GetRenderer() const { return *m_Renderer; }
        [[nodiscard]] VulkanFrameData& GetFrameData() const { return *m_FrameData; }
        [[nodiscard]] VulkanDeletionQueue& GetDeletionQueue() const { return *m_DeletionQueue; }
        [[nodiscard]] VulkanSamplerCache& GetSamplerCache() const { return *m_SamplerCache; }

        [[nodiscard]] uint32_t GetCurrentFrameIndex() const;
        [[nodiscard]] bool IsInFrame() const { return m_InFrame; }

        static VulkanContext& Get() { return *s_Instance; }
        static VulkanContext* TryGet() { return s_Instance; }

        static constexpr uint32_t GetFramesInFlight() { return MaxFramesInFlight; }


    private:
        void CreateSurface();
        bool RecreateSwapchain();

        [[nodiscard]] bool SwapchainMatchesWindow() const;

    private:
        GLFWwindow* m_WindowHandle;

        Scope<VulkanInstance> m_Instance;
        VkSurfaceKHR m_Surface = nullptr;
        Scope<VulkanDevice> m_Device;
        Scope<VulkanAllocator> m_Allocator;
        Scope<VulkanUploader> m_Uploader;
        Scope<VulkanDescriptorAllocator> m_DescriptorAllocator;
        Scope<VulkanSwapchain> m_Swapchain;
        Scope<VulkanCommandPool> m_CommandPool;
        Scope<VulkanSyncObjects> m_SyncObjects;
        Scope<VulkanFrameData> m_FrameData;
        Scope<VulkanRenderer> m_Renderer;
        Scope<VulkanDeletionQueue> m_DeletionQueue;
        Scope<VulkanSamplerCache> m_SamplerCache;

        bool m_FramebufferResized = false;
        bool m_InFrame = false;

        static VulkanContext* s_Instance;

        static constexpr uint32_t MaxFramesInFlight = 2;
    };
}
