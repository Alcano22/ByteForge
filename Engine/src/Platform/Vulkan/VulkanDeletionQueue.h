#pragma once

#include "Engine/Core/NonCopyable.h"

#include <cstdint>
#include <queue>
#include <functional>

namespace ByteForge
{
    class VulkanDeletionQueue : NonCopyable
    {
    public:
        explicit VulkanDeletionQueue(const uint32_t framesInFlight)
            : m_FramesInFlight(framesInFlight) {}

        void Push(std::function<void()> destroy);

        void OnFrameSubmitted() { ++m_SubmittedFrames; }

        void Collect();
        void Flush();

    private:
        void PopAndRun();

    private:
        struct Entry
        {
            uint64_t SubmittedFrames;
            std::function<void()> Destroy;
        };

        std::deque<Entry> m_Pending;
        uint32_t m_FramesInFlight;
        uint64_t m_SubmittedFrames = 0;
    };
}
