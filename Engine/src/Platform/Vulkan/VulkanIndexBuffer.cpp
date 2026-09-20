#include "Platform/Vulkan/VulkanIndexBuffer.h"

#include "VulkanUploader.h"
#include "Platform/Vulkan/VulkanContext.h"

namespace ByteForge
{
    VulkanIndexBuffer::VulkanIndexBuffer(const std::span<const uint32_t> indices)
        : m_Buffer(VulkanContext::Get().GetAllocator(), indices.size_bytes(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                   VulkanBufferMemory::DeviceLocal),
          m_Count(static_cast<uint32_t>(indices.size()))
    {
        VulkanContext::Get().GetUploader().UploadBuffer(m_Buffer, indices.data(), indices.size_bytes());
    }
}
