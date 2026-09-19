#pragma once

#include "Engine/Core/Core.h"

#include <spdlog/spdlog.h>

#include <memory>

namespace ByteForge
{
    class BYTEFORGE_API Log
    {
    public:
        static void Init();

        static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }
        static std::shared_ptr<spdlog::logger>& GetClientLogger() { return s_ClientLogger; }

    private:
        static std::shared_ptr<spdlog::logger> s_CoreLogger;
        static std::shared_ptr<spdlog::logger> s_ClientLogger;
    };
}

#define CORE_TRACE(...)    ::ByteForge::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define CORE_INFO(...)     ::ByteForge::Log::GetCoreLogger()->info(__VA_ARGS__)
#define CORE_WARN(...)     ::ByteForge::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define CORE_ERROR(...)    ::ByteForge::Log::GetCoreLogger()->error(__VA_ARGS__)
#define CORE_CRITICAL(...) ::ByteForge::Log::GetCoreLogger()->critical(__VA_ARGS__)

#define APP_TRACE(...)     ::ByteForge::Log::GetClientLogger()->trace(__VA_ARGS__)
#define APP_INFO(...)      ::ByteForge::Log::GetClientLogger()->info(__VA_ARGS__)
#define APP_WARN(...)      ::ByteForge::Log::GetClientLogger()->warn(__VA_ARGS__)
#define APP_ERROR(...)     ::ByteForge::Log::GetClientLogger()->error(__VA_ARGS__)
#define APP_CRITICAL(...)  ::ByteForge::Log::GetClientLogger()->critical(__VA_ARGS__)
