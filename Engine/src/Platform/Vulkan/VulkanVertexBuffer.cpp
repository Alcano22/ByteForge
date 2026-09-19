#include "Platform/Vulkan/VulkanVertexBuffer.h"
#include "Platform/Vulkan/VulkanContext.h"

namespace ByteForge
{
    VulkanVertexBuffer::VulkanVertexBuffer(const uint32_t size)
        : m_Buffer(VulkanContext::Get().GetAllocator(), size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) {}

    VulkanVertexBuffer::VulkanVertexBuffer(const void* vertices, const uint32_t size)
        : m_Buffer(VulkanContext::Get().GetAllocator(), size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)
    {
        m_Buffer.SetData(vertices, size);
    }

    void VulkanVertexBuffer::SetData(const void* data, const uint32_t size)
    {
        m_Buffer.SetData(data, size);
    }
}
