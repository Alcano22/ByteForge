#include "Platform/Vulkan/VulkanUniformBuffer.h"
#include "Platform/Vulkan/VulkanContext.h"

namespace ByteForge
{
    VulkanUniformBuffer::VulkanUniformBuffer(const u32 size)
        : m_Buffers(size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT), m_Size(size) {}

    void VulkanUniformBuffer::SetData(const void* data, const u32 size, const u32 offset)
    {
        m_Buffers.SetData(data, size, offset);
    }
}
