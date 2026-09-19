#include "Platform/Vulkan/VulkanSyncObjects.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanHelpers.h"
#include "Engine/Core/Log.h"

namespace ByteForge
{
    VulkanSyncObjects::VulkanSyncObjects(const VulkanDevice& device, const uint32_t frameCount,
                                         const uint32_t imageCount)
        : m_Device(device)
    {
        m_ImageAvailable.resize(frameCount);
        m_InFlightFences.resize(frameCount);
        m_RenderFinished.resize(imageCount);

        constexpr VkSemaphoreCreateInfo semaphoreInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
        };

        constexpr VkFenceCreateInfo fenceInfo{
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT
        };

        for (uint32_t i = 0; i < frameCount; ++i)
        {
            VK_CHECK(vkCreateSemaphore(m_Device.GetHandle(), &semaphoreInfo, nullptr, &m_ImageAvailable[i]));
            VK_CHECK(vkCreateFence(m_Device.GetHandle(), &fenceInfo, nullptr, &m_InFlightFences[i]));
        }

        for (uint32_t i = 0; i < imageCount; ++i)
            VK_CHECK(vkCreateSemaphore(m_Device.GetHandle(), &semaphoreInfo, nullptr, &m_RenderFinished[i]));

        CORE_INFO("Created sync objects for {} frames in flight, {} swapchain images", frameCount, imageCount);
    }

    VulkanSyncObjects::~VulkanSyncObjects()
    {
        for (const VkSemaphore semaphore : m_ImageAvailable)
            vkDestroySemaphore(m_Device.GetHandle(), semaphore, nullptr);

        for (const VkSemaphore semaphore : m_RenderFinished)
            vkDestroySemaphore(m_Device.GetHandle(), semaphore, nullptr);

        for (const VkFence fence : m_InFlightFences)
            vkDestroyFence(m_Device.GetHandle(), fence, nullptr);
    }
}
