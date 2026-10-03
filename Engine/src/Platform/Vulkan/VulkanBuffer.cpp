#include "Platform/Vulkan/VulkanBuffer.h"
#include "Platform/Vulkan/VulkanAllocator.h"
#include "Platform/Vulkan/VulkanHelpers.h"
#include "Engine/Core/Log.h"

#include <cstring>
#include <stdexcept>

namespace ByteForge
{
    VulkanBuffer::VulkanBuffer(const VulkanAllocator& allocator, const VkDeviceSize size,
                               const VkBufferUsageFlags usage, const VulkanBufferMemory memory)
        : m_Allocator(allocator), m_Size(size)
    {
        const bool deviceLocal = memory == VulkanBufferMemory::DeviceLocal;
        const VkBufferCreateInfo bufferInfo{
            .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size        = size,
            .usage       = deviceLocal ? usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT : usage,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE
        };

        VmaAllocationCreateInfo allocInfo{};
        switch (memory)
        {
            case VulkanBufferMemory::DeviceLocal:
                allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
                break;

            case VulkanBufferMemory::HostVisible:
                allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                                | VMA_ALLOCATION_CREATE_MAPPED_BIT;
                allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
                break;

            case VulkanBufferMemory::Readback:
                allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT
                                | VMA_ALLOCATION_CREATE_MAPPED_BIT;
                allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
                break;
        }

        VmaAllocationInfo allocationInfo;
        VK_CHECK(vmaCreateBuffer(m_Allocator.GetHandle(), &bufferInfo, &allocInfo,
                                 &m_Buffer, &m_Allocation, &allocationInfo));

        m_MappedData = allocationInfo.pMappedData;

        VkMemoryPropertyFlags memFlags = 0;
        vmaGetAllocationMemoryProperties(m_Allocator.GetHandle(), m_Allocation, &memFlags);

        CORE_TRACE("Vulkan buffer created ({} bytes, {}{})", size,
                   (memFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ? "device-local" : "system memory",
                   (memFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) ? ", host-visible" : "");
    }

    VulkanBuffer::~VulkanBuffer()
    {
        if (m_Buffer != nullptr)
            vmaDestroyBuffer(m_Allocator.GetHandle(), m_Buffer, m_Allocation);
    }

    void VulkanBuffer::SetData(const void* data, const size_t size, const size_t offset) const
    {
        if (m_MappedData == nullptr)
            throw std::runtime_error("VulkanBuffer::SetData: buffer is not host-visible, use VulkanUploader");

        if (offset > m_Size || size > m_Size - offset)
            throw std::runtime_error("VulkanBuffer::SetData: write exceeds buffer bounds");

        std::memcpy(static_cast<char*>(m_MappedData) + offset, data, size);

        VK_CHECK(vmaFlushAllocation(m_Allocator.GetHandle(), m_Allocation, offset, size));
    }

    void VulkanBuffer::GetData(void* data, const size_t size, const size_t offset) const
    {
        if (m_MappedData == nullptr)
            throw std::runtime_error("VulkanBuffer::GetData: buffer is not host-visible");

        if (offset > m_Size || size > m_Size - offset)
            throw std::runtime_error("VulkanBuffer::GetData: read exceeds buffer bounds");

        VK_CHECK(vmaInvalidateAllocation(m_Allocator.GetHandle(), m_Allocation, offset, size));
        std::memcpy(data, static_cast<const char*>(m_MappedData) + offset, size);
    }
}
