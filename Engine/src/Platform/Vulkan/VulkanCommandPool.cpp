#include "Platform/Vulkan/VulkanCommandPool.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanHelpers.h"
#include "Engine/Core/Log.h"

namespace ByteForge
{
    VulkanCommandPool::VulkanCommandPool(const VulkanDevice& device, const uint32_t bufferCount)
        : m_Device(device)
    {
        const VkCommandPoolCreateInfo poolInfo{
            .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = m_Device.GetQueueFamilyIndices().GraphicsFamily.value()
        };

        VK_CHECK(vkCreateCommandPool(m_Device.GetHandle(), &poolInfo, nullptr, &m_CommandPool));

        m_CommandBuffers.resize(bufferCount);

        const VkCommandBufferAllocateInfo allocInfo{
            .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool        = m_CommandPool,
            .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = bufferCount
        };

        VK_CHECK(vkAllocateCommandBuffers(m_Device.GetHandle(), &allocInfo, m_CommandBuffers.data()));

        CORE_INFO("Created command pool with {} command buffers", bufferCount);
    }

    VulkanCommandPool::~VulkanCommandPool()
    {
        if (m_CommandPool != nullptr)
            vkDestroyCommandPool(m_Device.GetHandle(), m_CommandPool, nullptr);
    }
}
