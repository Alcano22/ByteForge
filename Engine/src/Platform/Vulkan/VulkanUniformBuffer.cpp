#include "Platform/Vulkan/VulkanUniformBuffer.h"
#include "Platform/Vulkan/VulkanContext.h"

namespace ByteForge
{
    VulkanUniformBuffer::VulkanUniformBuffer(const uint32_t size)
        : m_Size(size)
    {
        const uint32_t framesInFlight = VulkanContext::GetFramesInFlight();
        m_Buffers.reserve(framesInFlight);

        for (uint32_t i = 0; i < framesInFlight; ++i)
        {
            m_Buffers.push_back(MakeScope<VulkanBuffer>(
                VulkanContext::Get().GetAllocator(), size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT));
        }
    }

    void VulkanUniformBuffer::SetData(const void* data, const uint32_t size, const uint32_t offset)
    {
        const uint32_t frame = VulkanContext::Get().GetCurrentFrameIndex();
        m_Buffers[frame]->SetData(data, size, offset);
    }
}
