#pragma once

#include "Engine/Renderer/GraphicsContext.h"

#include <vulkan/vulkan.h>

#include <memory>

#include "VulkanInstance.h"

struct GLFWwindow;

namespace ByteForge
{
    class VulkanInstance;
    class VulkanDevice;
    class VulkanAllocator;
    class VulkanSwapchain;
    class VulkanRenderPass;
    class VulkanFramebuffers;
    class VulkanCommandPool;
    class VulkanSyncObjects;
    class VulkanRenderer;

    class VulkanContext : public GraphicsContext
    {
    public:
        explicit VulkanContext(GLFWwindow* windowHandle);
        ~VulkanContext() override;

        void Init() override;

        void BeginFrame();
        void EndFrame();

        void NotifyFramebufferResized() { m_FramebufferResized = true; }

        [[nodiscard]] VkInstance GetInstanceHandle() const { return m_Instance->GetHandle(); }
        [[nodiscard]] VulkanDevice& GetDevice() const { return *m_Device; }
        [[nodiscard]] VulkanAllocator& GetAllocator() const { return *m_Allocator; }
        [[nodiscard]] VulkanSwapchain& GetSwapchain() const { return *m_Swapchain; }
        [[nodiscard]] VulkanRenderPass& GetRenderPass() const { return *m_RenderPass; }
        [[nodiscard]] VulkanRenderPass& GetImGuiRenderPass() const { return *m_ImGuiRenderPass; }
        [[nodiscard]] VulkanRenderer& GetRenderer() const { return *m_Renderer; }
        [[nodiscard]] uint32_t GetCurrentFrameIndex() const;

        static VulkanContext& Get() { return *s_Instance; }

        static constexpr uint32_t GetFramesInFlight() { return MaxFramesInFlight; }

    private:
        void CreateSurface();
        void RecreateSwapchain();

    private:
        GLFWwindow* m_WindowHandle;

        Scope<VulkanInstance> m_Instance;
        VkSurfaceKHR m_Surface = nullptr;
        Scope<VulkanDevice> m_Device;
        Scope<VulkanAllocator> m_Allocator;
        Scope<VulkanSwapchain> m_Swapchain;
        Scope<VulkanRenderPass> m_RenderPass;
        Scope<VulkanFramebuffers> m_Framebuffers;
        Scope<VulkanRenderPass> m_ImGuiRenderPass;
        Scope<VulkanFramebuffers> m_ImGuiFramebuffers;
        Scope<VulkanCommandPool> m_CommandPool;
        Scope<VulkanSyncObjects> m_SyncObjects;
        Scope<VulkanRenderer> m_Renderer;

        bool m_FramebufferResized = false;

        static VulkanContext* s_Instance;

        static constexpr uint32_t MaxFramesInFlight = 2;
    };
}
