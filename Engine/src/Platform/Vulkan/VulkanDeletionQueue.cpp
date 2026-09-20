#include "Platform/Vulkan/VulkanDeletionQueue.h"

namespace ByteForge
{
    void VulkanDeletionQueue::Push(std::function<void()> destroy)
    {
        m_Pending.push_back({ m_SubmittedFrames, std::move(destroy) });
    }

    void VulkanDeletionQueue::Collect()
    {
        while (!m_Pending.empty() && m_Pending.front().SubmittedFrames + m_FramesInFlight <= m_SubmittedFrames)
            PopAndRun();
    }

    void VulkanDeletionQueue::Flush()
    {
        while (!m_Pending.empty())
            PopAndRun();
    }

    void VulkanDeletionQueue::PopAndRun()
    {
        const auto destroy = std::move(m_Pending.front().Destroy);
        m_Pending.pop_front();
        destroy();
    }
}
