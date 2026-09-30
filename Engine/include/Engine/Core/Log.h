#pragma once

#include "Engine/Core/Core.h"

#include <spdlog/spdlog.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>

namespace ByteForge
{
    enum class LogLevel { Trace, Debug, Info, Warn, Error, Critical };

    struct LogEntry
    {
        uint64_t Id = 0;
        LogLevel Level = LogLevel::Info;
        std::string Logger;
        std::string Message;
        std::chrono::system_clock::time_point Time;
        std::string File;
        int Line = 0;
        std::string Function;
        size_t ThreadId = 0;
    };

    using LogCallback = std::function<void(const LogEntry&)>;

    class BYTEFORGE_API LogSubscription
    {
    public:
        LogSubscription() = default;
        ~LogSubscription() { Reset(); }

        LogSubscription(LogSubscription&& other) noexcept
            : m_Id(std::exchange(other.m_Id, 0)) {}

        LogSubscription& operator=(LogSubscription&& other) noexcept
        {
            if (this != &other)
            {
                Reset();
                m_Id = std::exchange(other.m_Id, 0);
            }
            return *this;
        }

        LogSubscription(const LogSubscription&) = delete;
        LogSubscription& operator=(const LogSubscription&) = delete;

        void Reset();

    private:
        explicit LogSubscription(const uint64_t id)
            : m_Id(id) {}

    private:
        friend class Log;

        uint64_t m_Id = 0;
    };

    class BYTEFORGE_API Log
    {
    public:
        static void Init();

        [[nodiscard]] static LogSubscription Subscribe(LogCallback callback, bool replayHistory = true);

        static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }
        static std::shared_ptr<spdlog::logger>& GetClientLogger() { return s_ClientLogger; }

    private:
        static void Unsubscribe(uint64_t id);

    private:
        friend class LogSubscription;

        static std::shared_ptr<spdlog::logger> s_CoreLogger;
        static std::shared_ptr<spdlog::logger> s_ClientLogger;
    };
}

#define BYTEFORGE_LOG_CALL(logger, level, ...) \
    (logger)->log(::spdlog::source_loc{ __FILE__, __LINE__, SPDLOG_FUNCTION }, level, __VA_ARGS__)

#define CORE_TRACE(...)    BYTEFORGE_LOG_CALL(::ByteForge::Log::GetCoreLogger(), ::spdlog::level::trace, __VA_ARGS__)
#define CORE_DEBUG(...)    BYTEFORGE_LOG_CALL(::ByteForge::Log::GetCoreLogger(), ::spdlog::level::debug, __VA_ARGS__)
#define CORE_INFO(...)     BYTEFORGE_LOG_CALL(::ByteForge::Log::GetCoreLogger(), ::spdlog::level::info, __VA_ARGS__)
#define CORE_WARN(...)     BYTEFORGE_LOG_CALL(::ByteForge::Log::GetCoreLogger(), ::spdlog::level::warn, __VA_ARGS__)
#define CORE_ERROR(...)    BYTEFORGE_LOG_CALL(::ByteForge::Log::GetCoreLogger(), ::spdlog::level::err, __VA_ARGS__)
#define CORE_CRITICAL(...) BYTEFORGE_LOG_CALL(::ByteForge::Log::GetCoreLogger(), ::spdlog::level::critical, __VA_ARGS__)

#define APP_TRACE(...)     BYTEFORGE_LOG_CALL(::ByteForge::Log::GetClientLogger(), ::spdlog::level::trace, __VA_ARGS__)
#define APP_DEBUG(...)     BYTEFORGE_LOG_CALL(::ByteForge::Log::GetClientLogger(), ::spdlog::level::debug, __VA_ARGS__)
#define APP_INFO(...)      BYTEFORGE_LOG_CALL(::ByteForge::Log::GetClientLogger(), ::spdlog::level::info, __VA_ARGS__)
#define APP_WARN(...)      BYTEFORGE_LOG_CALL(::ByteForge::Log::GetClientLogger(), ::spdlog::level::warn, __VA_ARGS__)
#define APP_ERROR(...)     BYTEFORGE_LOG_CALL(::ByteForge::Log::GetClientLogger(), ::spdlog::level::err, __VA_ARGS__)
#define APP_CRITICAL(...)  BYTEFORGE_LOG_CALL(::ByteForge::Log::GetClientLogger(), ::spdlog::level::critical, __VA_ARGS__)
