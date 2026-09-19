#include "Platform/Vulkan/VulkanBuffer.h"
#include "Platform/Vulkan/VulkanAllocator.h"
#include "Platform/Vulkan/VulkanHelpers.h"
#include "Engine/Core/Log.h"

#include <cstring>
#include <stdexcept>

namespace ByteForge
{
    VulkanBuffer::VulkanBuffer(const VulkanAllocator& allocator,
                               const VkDeviceSize size, const VkBufferUsageFlags usage)
        : m_Allocator(allocator), m_Size(size)
    {
        const VkBufferCreateInfo bufferInfo{
            .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size        = size,
            .usage       = usage,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE
        };

        constexpr VmaAllocationCreateInfo allocInfo{
            .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                   | VMA_ALLOCATION_CREATE_MAPPED_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO
        };

        VmaAllocationInfo allocationInfo;
        VK_CHECK(vmaCreateBuffer(m_Allocator.GetHandle(), &bufferInfo, &allocInfo,
                                 &m_Buffer, &m_Allocation, &allocationInfo));

        m_MappedData = allocationInfo.pMappedData;

        CORE_INFO("Vulkan buffer created ({} bytes)", size);
    }

    VulkanBuffer::~VulkanBuffer()
    {
        if (m_Buffer != nullptr)
            vmaDestroyBuffer(m_Allocator.GetHandle(), m_Buffer, m_Allocation);
    }

    void VulkanBuffer::SetData(const void* data, const size_t size, const size_t offset) const
    {
        if (offset + size > m_Size)
            throw std::runtime_error("VulkanBuffer::SetData: write exceeds buffer bounds");

        std::memcpy(static_cast<char*>(m_MappedData) + offset, data, size);
    }
}
