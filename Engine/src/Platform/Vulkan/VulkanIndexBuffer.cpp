#include "Platform/Vulkan/VulkanIndexBuffer.h"

#include "VulkanUploader.h"
#include "Platform/Vulkan/VulkanContext.h"

namespace ByteForge
{
    VulkanIndexBuffer::VulkanIndexBuffer(const std::span<const u32> indices)
        : m_Buffer(VulkanContext::Get().GetAllocator(), indices.size_bytes(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                   VulkanBufferMemory::DeviceLocal),
          m_Count(static_cast<u32>(indices.size()))
    {
        VulkanContext::Get().GetUploader().UploadBuffer(m_Buffer, indices.data(), indices.size_bytes());
    }
}
