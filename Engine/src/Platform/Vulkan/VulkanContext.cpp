#include "Platform/Vulkan/VulkanContext.h"
#include "Platform/Vulkan/VulkanInstance.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanAllocator.h"
#include "Platform/Vulkan/VulkanUploader.h"
#include "Platform/Vulkan/VulkanDescriptorAllocator.h"
#include "Platform/Vulkan/VulkanSwapchain.h"
#include "Platform/Vulkan/VulkanCommandPool.h"
#include "Platform/Vulkan/VulkanSyncObjects.h"
#include "Platform/Vulkan/VulkanFrameData.h"
#include "Platform/Vulkan/VulkanRenderer.h"
#include "Platform/Vulkan/VulkanDeletionQueue.h"
#include "Platform/Vulkan/VulkanHelpers.h"
#include "Engine/Core/Log.h"

#include <GLFW/glfw3.h>

namespace ByteForge
{
    VulkanContext* VulkanContext::s_Instance = nullptr;

    VulkanContext::VulkanContext(GLFWwindow* windowHandle)
        : m_WindowHandle(windowHandle) {}

    VulkanContext::~VulkanContext()
    {
        if (m_Device)
            m_Device->WaitIdle();

        if (m_DeletionQueue)
            m_DeletionQueue->Flush();

        s_Instance = nullptr;

        m_Renderer.reset();
        m_FrameData.reset();
        m_SyncObjects.reset();
        m_CommandPool.reset();
        m_Swapchain.reset();
        m_DescriptorAllocator.reset();
        m_Uploader.reset();
        m_Allocator.reset();
        m_Device.reset();

        if (m_Surface != nullptr && m_Instance)
            vkDestroySurfaceKHR(m_Instance->GetHandle(), m_Surface, nullptr);

        m_Instance.reset();
    }

    void VulkanContext::Init()
    {
        s_Instance = this;

        m_DeletionQueue = MakeScope<VulkanDeletionQueue>(MaxFramesInFlight);

        CORE_INFO("Initializing Vulkan context");

        m_Instance = MakeScope<VulkanInstance>();
        CreateSurface();
        m_Device = MakeScope<VulkanDevice>(m_Instance->GetHandle(), m_Surface);
        m_Allocator = MakeScope<VulkanAllocator>(m_Instance->GetHandle(), *m_Device);
        m_Uploader = MakeScope<VulkanUploader>(*m_Device, *m_Allocator);
        m_DescriptorAllocator = MakeScope<VulkanDescriptorAllocator>(*m_Device);
        m_Swapchain = MakeScope<VulkanSwapchain>(*m_Device, m_Surface, m_WindowHandle);
        m_CommandPool = MakeScope<VulkanCommandPool>(*m_Device, MaxFramesInFlight);
        m_SyncObjects = MakeScope<VulkanSyncObjects>(*m_Device, MaxFramesInFlight,
                                                     static_cast<uint32_t>(m_Swapchain->GetImages().size()));

        m_FrameData = MakeScope<VulkanFrameData>(*m_Device);

        m_Renderer = MakeScope<VulkanRenderer>(*m_Device, *m_Swapchain, *m_CommandPool,
                                               *m_SyncObjects, *m_FrameData, MaxFramesInFlight);
    }

    void VulkanContext::BeginFrame()
    {
        m_InFrame = true;

        if (m_FramebufferResized)
        {
            m_FramebufferResized = false;
            RecreateSwapchain();
        }

        if (m_Renderer->BeginFrame() == VulkanRenderer::FrameResult::NeedsRecreation)
        {
            RecreateSwapchain();
            m_Renderer->BeginFrame();
        }

        m_FrameData->BeginFrame();
        m_DeletionQueue->Collect();
    }

    void VulkanContext::EndFrame()
    {
        m_InFrame = false;

        const bool willSubmit = !m_Renderer->IsFrameSkipped();
        const auto result = m_Renderer->EndFrame();

        if (willSubmit)
            m_DeletionQueue->OnFrameSubmitted();

        if (result == VulkanRenderer::FrameResult::NeedsRecreation)
            RecreateSwapchain();
    }

    void VulkanContext::CreateSurface()
    {
        VK_CHECK(glfwCreateWindowSurface(m_Instance->GetHandle(), m_WindowHandle, nullptr, &m_Surface));

        CORE_INFO("Vulkan surface created");
    }

    void VulkanContext::RecreateSwapchain()
    {
        int width = 0, height = 0;
        glfwGetFramebufferSize(m_WindowHandle, &width, &height);
        while (width == 0 || height == 0)
        {
            glfwGetFramebufferSize(m_WindowHandle, &width, &height);
            glfwWaitEvents();
        }

        m_Device->WaitIdle();

        m_Swapchain.reset();
        m_Swapchain = MakeScope<VulkanSwapchain>(*m_Device, m_Surface, m_WindowHandle);

        m_Renderer->UpdateSwapchain(*m_Swapchain);

        CORE_INFO("Swapchain recreated ({}x{})", width, height);
    }

    uint32_t VulkanContext::GetCurrentFrameIndex() const { return m_Renderer->GetCurrentFrameIndex(); }
}
