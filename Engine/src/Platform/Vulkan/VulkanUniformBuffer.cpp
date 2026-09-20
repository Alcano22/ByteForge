#include "Platform/Vulkan/VulkanUniformBuffer.h"
#include "Platform/Vulkan/VulkanContext.h"

namespace ByteForge
{
    VulkanUniformBuffer::VulkanUniformBuffer(const uint32_t size)
        : m_Buffers(size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT), m_Size(size) {}

    void VulkanUniformBuffer::SetData(const void* data, const uint32_t size, const uint32_t offset)
    {
        m_Buffers.SetData(data, size, offset);
    }
}
