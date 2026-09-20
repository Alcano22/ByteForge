#include "Platform/Vulkan/VulkanPerFrameBuffer.h"
#include "Platform/Vulkan/VulkanContext.h"

#include <stdexcept>

namespace ByteForge
{
    VulkanPerFrameBuffer::VulkanPerFrameBuffer(const VkDeviceSize size, const VkBufferUsageFlags usage)
    {
        constexpr uint32_t framesInFlight = VulkanContext::GetFramesInFlight();
        m_Buffers.reserve(framesInFlight);

        for (uint32_t i = 0; i < framesInFlight; ++i)
            m_Buffers.push_back(MakeScope<VulkanBuffer>(VulkanContext::Get().GetAllocator(), size, usage));
    }

    void VulkanPerFrameBuffer::SetData(const void* data, const size_t size, const size_t offset) const
    {
        const VulkanContext& context = VulkanContext::Get();
        if (!context.IsInFrame())
        {
            throw std::runtime_error("Per-frame buffers can only be written between Renderer::BeginFrame and "
                                     "Renderer::EndFrame (e.g. in Layer::OnUpdate)");
        }

        m_Buffers[context.GetCurrentFrameIndex()]->SetData(data, size, offset);
    }

    VkBuffer VulkanPerFrameBuffer::GetCurrentHandle() const
    {
        return GetHandle(VulkanContext::Get().GetCurrentFrameIndex());
    }
}
