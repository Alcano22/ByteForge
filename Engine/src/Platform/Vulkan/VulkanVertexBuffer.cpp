#include "Platform/Vulkan/VulkanVertexBuffer.h"
#include "Platform/Vulkan/VulkanContext.h"
#include "Platform/Vulkan/VulkanFrameData.h"
#include "Platform/Vulkan/VulkanUploader.h"

#include <stdexcept>

namespace ByteForge
{
    VulkanVertexBuffer::VulkanVertexBuffer(const uint32_t size)
        : m_StreamingBuffer(MakeScope<VulkanPerFrameBuffer>(size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)) {}

    VulkanVertexBuffer::VulkanVertexBuffer(const void* vertices, const uint32_t size)
        : m_StaticBuffer(MakeScope<VulkanBuffer>(VulkanContext::Get().GetAllocator(), size,
                                                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VulkanBufferMemory::DeviceLocal))
    {
        VulkanContext::Get().GetUploader().UploadBuffer(*m_StaticBuffer, vertices, size);
    }

    void VulkanVertexBuffer::SetData(const void* data, const uint32_t size, const uint32_t offset)
    {
        if (!m_StreamingBuffer)
        {
            throw std::runtime_error("VertexBuffer::SetData: this buffer was created with initial data and is "
                                     "immutable, use VertexBuffer::Create(size) for per-frame data");
        }

        m_StreamingBuffer->SetData(data, size, offset);
        m_LastWriteFrame = VulkanContext::Get().GetFrameData().GetFrameNumber();
    }

    VkBuffer VulkanVertexBuffer::GetHandleForDraw() const
    {
        if (m_StaticBuffer)
            return m_StaticBuffer->GetHandle();

        if (m_LastWriteFrame != VulkanContext::Get().GetFrameData().GetFrameNumber())
        {
            throw std::runtime_error("VertexBuffer: a per-frame buffer was drawn in a frame in which SetData was "
                                     "not called; upload its data every frame, or create it with initial data "
                                     "for static geometry");
        }

        return m_StreamingBuffer->GetCurrentHandle();
    }
}
