#include "Platform/Vulkan/VulkanSyncObjects.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanHelpers.h"
#include "Engine/Core/Log.h"

namespace ByteForge
{
    namespace
    {
        constexpr VkSemaphoreCreateInfo SemaphoreInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
        };
    }

    VulkanSyncObjects::VulkanSyncObjects(const VulkanDevice& device, const uint32_t frameCount,
                                         const uint32_t imageCount)
        : m_Device(device)
    {
        m_ImageAvailable.resize(frameCount);
        m_InFlightFences.resize(frameCount);

        constexpr VkFenceCreateInfo fenceInfo{
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT
        };

        for (uint32_t i = 0; i < frameCount; ++i)
        {
            VK_CHECK(vkCreateSemaphore(m_Device.GetHandle(), &SemaphoreInfo, nullptr, &m_ImageAvailable[i]));
            VK_CHECK(vkCreateFence(m_Device.GetHandle(), &fenceInfo, nullptr, &m_InFlightFences[i]));
        }

        CreateRenderFinished(imageCount);

        CORE_INFO("Created sync objects for {} frames in flight, {} swapchain images", frameCount, imageCount);
    }

    VulkanSyncObjects::~VulkanSyncObjects()
    {
        for (const VkSemaphore semaphore : m_ImageAvailable)
            vkDestroySemaphore(m_Device.GetHandle(), semaphore, nullptr);

        DestroyRenderFinished();

        for (const VkFence fence : m_InFlightFences)
            vkDestroyFence(m_Device.GetHandle(), fence, nullptr);
    }

    void VulkanSyncObjects::RecreateRenderFinished(const uint32_t imageCount)
    {
        DestroyRenderFinished();
        CreateRenderFinished(imageCount);

        CORE_TRACE("Recreated {} render-finished semaphores", imageCount);
    }

    void VulkanSyncObjects::CreateRenderFinished(const uint32_t imageCount)
    {
        m_RenderFinished.assign(imageCount, nullptr);

        for (VkSemaphore& semaphore : m_RenderFinished)
            VK_CHECK(vkCreateSemaphore(m_Device.GetHandle(), &SemaphoreInfo, nullptr, &semaphore));
    }

    void VulkanSyncObjects::DestroyRenderFinished()
    {
        for (const VkSemaphore semaphore : m_RenderFinished)
            vkDestroySemaphore(m_Device.GetHandle(), semaphore, nullptr);

        m_RenderFinished.clear();
    }
}
