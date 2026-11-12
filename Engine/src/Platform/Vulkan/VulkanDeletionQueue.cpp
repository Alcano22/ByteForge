#include "Platform/Vulkan/VulkanDeletionQueue.h"
#include "Engine/Core/Log.h"

#include <exception>

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

        try
        {
            destroy();
        } catch (const std::exception& e)
        {
            CORE_ERROR("VulkanDeletionQueue: a deferred cleanup failed: {}", e.what());
        }
    }
}
