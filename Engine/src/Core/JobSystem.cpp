#include "Engine/Core/JobSystem.h"
#include "Engine/Core/Log.h"

#include <algorithm>
#include <utility>

namespace ByteForge
{
    std::vector<std::jthread> JobSystem::s_Workers;
    std::queue<std::function<void()>> JobSystem::s_Tasks;
    std::mutex JobSystem::s_Mutex;
    std::condition_variable JobSystem::s_Condition;
    bool JobSystem::s_Stop = false;

    void JobSystem::Init(u32 threadCount)
    {
        if (!s_Workers.empty())
            throw std::runtime_error("JobSystem: already initialized");

        if (threadCount == 0)
            threadCount = std::max(2u, std::thread::hardware_concurrency()) - 1;

        {
            std::scoped_lock lock(s_Mutex);
            s_Stop = false;
        }

        s_Workers.reserve(threadCount);
        for (u32 i = 0; i < threadCount; ++i)
            s_Workers.emplace_back(&JobSystem::WorkerLoop);

        CORE_INFO("JobSystem initialized with {} worker threads", threadCount);
    }

    void JobSystem::Shutdown()
    {
        std::queue<std::function<void()>> discarded;
        {
            std::scoped_lock lock(s_Mutex);
            if (s_Workers.empty()) return;

            s_Stop = true;
            std::swap(discarded, s_Tasks);
        }

        s_Condition.notify_all();

        s_Workers.clear();

        const usize count = discarded.size();
        discarded = {};

        CORE_INFO("JobSystem shut down ({} queued job(s) discarded)", count);
    }

    void JobSystem::WorkerLoop()
    {
        while (true)
        {
            std::function<void()> task;
            {
                std::unique_lock lock(s_Mutex);
                s_Condition.wait(lock, [] { return s_Stop || !s_Tasks.empty(); });

                if (s_Stop) return;

                task = std::move(s_Tasks.front());
                s_Tasks.pop();
            }

            task();
        }
    }
}
