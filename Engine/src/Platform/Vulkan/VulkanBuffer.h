#pragma once

#include "Engine/Core/NonCopyable.h"

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include <cstddef>

namespace ByteForge
{
    class VulkanAllocator;

    class VulkanBuffer : NonCopyable
    {
    public:
        VulkanBuffer(const VulkanAllocator& allocator, VkDeviceSize size, VkBufferUsageFlags usage);
        ~VulkanBuffer();

        void SetData(const void* data, size_t size, size_t offset = 0) const;

        [[nodiscard]] VkBuffer GetHandle() const { return m_Buffer; }

    private:
        const VulkanAllocator& m_Allocator;
        VkBuffer m_Buffer = nullptr;
        VmaAllocation m_Allocation = nullptr;
        void* m_MappedData = nullptr;
        VkDeviceSize m_Size;
    };
}
