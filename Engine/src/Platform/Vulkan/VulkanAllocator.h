#pragma once

#include "Engine/Core/NonCopyable.h"

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

namespace ByteForge
{
    class VulkanDevice;

    class VulkanAllocator : NonCopyable
    {
    public:
        VulkanAllocator(VkInstance instance, const VulkanDevice& device);
        ~VulkanAllocator();

        [[nodiscard]] VmaAllocator GetHandle() const { return m_Allocator; }

    private:
        VmaAllocator m_Allocator = nullptr;
    };
}
