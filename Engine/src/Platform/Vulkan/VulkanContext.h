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
    class VulkanUploader;
    class VulkanDescriptorAllocator;
    class VulkanSwapchain;
    class VulkanCommandPool;
    class VulkanSyncObjects;
    class VulkanFrameData;
    class VulkanRenderer;
    class VulkanDeletionQueue;

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
        [[nodiscard]] VulkanUploader& GetUploader() const { return *m_Uploader; }
        [[nodiscard]] VulkanDescriptorAllocator& GetDescriptorAllocator() const { return *m_DescriptorAllocator; }
        [[nodiscard]] VulkanSwapchain& GetSwapchain() const { return *m_Swapchain; }
        [[nodiscard]] VulkanRenderer& GetRenderer() const { return *m_Renderer; }
        [[nodiscard]] VulkanFrameData& GetFrameData() const { return *m_FrameData; }
        [[nodiscard]] VulkanDeletionQueue& GetDeletionQueue() const { return *m_DeletionQueue; }

        [[nodiscard]] uint32_t GetCurrentFrameIndex() const;
        [[nodiscard]] bool IsInFrame() const { return m_InFrame; }

        static VulkanContext& Get() { return *s_Instance; }
        static VulkanContext* TryGet() { return s_Instance; }

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
        Scope<VulkanUploader> m_Uploader;
        Scope<VulkanDescriptorAllocator> m_DescriptorAllocator;
        Scope<VulkanSwapchain> m_Swapchain;
        Scope<VulkanCommandPool> m_CommandPool;
        Scope<VulkanSyncObjects> m_SyncObjects;
        Scope<VulkanFrameData> m_FrameData;
        Scope<VulkanRenderer> m_Renderer;
        Scope<VulkanDeletionQueue> m_DeletionQueue;

        bool m_FramebufferResized = false;
        bool m_InFrame = false;

        static VulkanContext* s_Instance;

        static constexpr uint32_t MaxFramesInFlight = 2;
    };
}
