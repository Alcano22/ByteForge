#pragma once

#include "Engine/Core/Core.h"

#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <vector>

namespace ByteForge
{
    class BYTEFORGE_API JobSystem
    {
    public:
        static void Init(u32 threadCount = 0);
        static void Shutdown();

        template<typename F>
        static std::future<std::invoke_result_t<F>> Submit(F&& job)
        {
            using ReturnType = std::invoke_result_t<F>;

            auto task = std::make_shared<std::packaged_task<ReturnType()>>(std::forward<F>(job));
            std::future<ReturnType> result = task->get_future();

            {
                std::scoped_lock lock(s_Mutex);
                if (s_Stop || s_Workers.empty())
                    throw std::runtime_error("JobSystem: cannot submit, the pool is not running");

                s_Tasks.emplace([task] { (*task)(); });
            }

            s_Condition.notify_one();
            return result;
        }

    private:
        static void WorkerLoop();

    private:
        static std::vector<std::jthread> s_Workers;
        static std::queue<std::function<void()>> s_Tasks;
        static std::mutex s_Mutex;
        static std::condition_variable s_Condition;
        static bool s_Stop;
    };
}
