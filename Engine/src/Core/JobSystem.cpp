#include "Engine/Core/JobSystem.h"
#include "Engine/Core/Log.h"

#include <algorithm>

namespace ByteForge
{
    std::vector<std::jthread> JobSystem::s_Workers;
    std::queue<std::function<void()>> JobSystem::s_Tasks;
    std::mutex JobSystem::s_Mutex;
    std::condition_variable_any JobSystem::s_Condition;
    bool JobSystem::s_Stop = false;

    void JobSystem::Init(uint32_t threadCount)
    {
        if (threadCount == 0)
            threadCount = std::max(2u, std::thread::hardware_concurrency()) - 1;

        s_Stop = false;
        s_Workers.reserve(threadCount);

        for (uint32_t i = 0; i < threadCount; ++i)
        {
            s_Workers.emplace_back([](std::stop_token stopToken)
            {
                while (!stopToken.stop_requested())
                {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(s_Mutex);
                        s_Condition.wait(lock, stopToken, [] { return s_Stop || !s_Tasks.empty(); });

                        if (s_Stop && s_Tasks.empty()) return;
                        if (s_Tasks.empty()) continue;

                        task = std::move(s_Tasks.front());
                        s_Tasks.pop();
                    }

                    task();
                }
            });
        }

        CORE_INFO("JobSystem initialized with {} worker threads", threadCount);
    }

    void JobSystem::Shutdown()
    {
        {
            std::unique_lock<std::mutex> lock(s_Mutex);
            s_Stop = true;
        }

        s_Condition.notify_all();

        s_Workers.clear();

        CORE_INFO("JobSystem shut down");
    }
}
