#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include "Platform/Vulkan/VulkanAllocator.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanHelpers.h"
#include "Engine/Core/Log.h"

namespace ByteForge
{
    VulkanAllocator::VulkanAllocator(const VkInstance instance, const VulkanDevice& device)
    {
        const VmaAllocatorCreateInfo createInfo{
            .physicalDevice   = device.GetPhysicalDevice(),
            .device           = device.GetHandle(),
            .instance         = instance,
            .vulkanApiVersion = VK_API_VERSION_1_3
        };

        VK_CHECK(vmaCreateAllocator(&createInfo, &m_Allocator));

        CORE_INFO("Vulkan memory allocator (VMA) created");
    }

    VulkanAllocator::~VulkanAllocator()
    {
        if (m_Allocator != nullptr)
            vmaDestroyAllocator(m_Allocator);
    }
}
