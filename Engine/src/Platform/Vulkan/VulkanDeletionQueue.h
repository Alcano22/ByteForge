#pragma once

#include "Engine/Core/NonCopyable.h"

#include <cstdint>
#include <deque>
#include <functional>

namespace ByteForge
{
    class VulkanDeletionQueue : NonCopyable
    {
    public:
        explicit VulkanDeletionQueue(const u32 framesInFlight)
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
            u64 SubmittedFrames;
            std::function<void()> Destroy;
        };

        std::deque<Entry> m_Pending;
        u32 m_FramesInFlight;
        u64 m_SubmittedFrames = 0;
    };
}
