#include "Engine/Core/Log.h"

#include <spdlog/sinks/base_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/details/null_mutex.h>

#include <algorithm>
#include <deque>
#include <mutex>
#include <stdexcept>
#include <vector>

namespace ByteForge
{
    namespace
    {
        constexpr size_t MaxHistory = 2000;

        struct LogState
        {
            std::mutex Mutex;
            std::deque<LogEntry> History;
            std::vector<std::pair<uint64_t, LogCallback>> Callbacks;
            uint64_t NextEntryId = 1;
            uint64_t NextSubscriptionId = 1;
        };

        LogState& GetState()
        {
            static LogState state;
            return state;
        }

        thread_local bool t_Dispatching = false;

        class DispatchScope
        {
        public:
            DispatchScope() { t_Dispatching = true; }
            ~DispatchScope() { t_Dispatching = false; }

            DispatchScope(const DispatchScope&) = delete;
            DispatchScope& operator=(const DispatchScope&) = delete;
        };

        LogLevel ToLogLevel(const spdlog::level::level_enum level)
        {
            switch (level)
            {
                case spdlog::level::trace:    return LogLevel::Trace;
                case spdlog::level::debug:    return LogLevel::Debug;
                case spdlog::level::info:     return LogLevel::Info;
                case spdlog::level::warn:     return LogLevel::Warn;
                case spdlog::level::err:      return LogLevel::Error;
                case spdlog::level::critical: return LogLevel::Critical;
                default:                      return LogLevel::Info;
            }
        }

        void Dispatch(LogEntry entry)
        {
            if (t_Dispatching) return;

            LogState& state = GetState();
            const std::lock_guard lock(state.Mutex);

            entry.Id = state.NextEntryId++;

            {
                const DispatchScope scope;
                for (const auto& [id, callback] : state.Callbacks)
                    callback(entry);
            }

            state.History.push_back(std::move(entry));
            if (state.History.size() > MaxHistory)
                state.History.pop_front();
        }

        class CallbackSink final : public spdlog::sinks::base_sink<spdlog::details::null_mutex>
        {
        protected:
            void sink_it_(const spdlog::details::log_msg& msg) override
            {
                LogEntry entry{
                    .Level    = ToLogLevel(msg.level),
                    .Logger   = std::string(msg.logger_name.data(), msg.logger_name.size()),
                    .Message  = std::string(msg.payload.data(), msg.payload.size()),
                    .Time     = msg.time,
                    .ThreadId = msg.thread_id
                };

                if (!msg.source.empty())
                {
                    entry.File = msg.source.filename;
                    entry.Line = msg.source.line;
                    if (msg.source.funcname != nullptr)
                        entry.Function = msg.source.funcname;
                }

                Dispatch(std::move(entry));
            }

            void flush_() override {}
        };
    }

    std::shared_ptr<spdlog::logger> Log::s_CoreLogger;
    std::shared_ptr<spdlog::logger> Log::s_ClientLogger;

    void Log::Init()
    {
        const auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        consoleSink->set_pattern("%^[%T] %n: %v%$");

        const auto callbackSink = std::make_shared<CallbackSink>();

        const auto createLogger = [&](const char* name)
        {
            auto logger = std::make_shared<spdlog::logger>(name, spdlog::sinks_init_list{ consoleSink, callbackSink });
            logger->set_level(spdlog::level::trace);
            spdlog::register_logger(logger);
            return logger;
        };

        s_CoreLogger = createLogger("ENGINE");
        s_ClientLogger = createLogger("APP");
    }

    LogSubscription Log::Subscribe(LogCallback callback, const bool replayHistory)
    {
        if (!callback)
            throw std::runtime_error("Log::Subscribe: the callback must not be empty");

        LogState& state = GetState();
        const std::lock_guard lock(state.Mutex);

        if (replayHistory)
        {
            const DispatchScope scope;
            for (const LogEntry& entry : state.History)
                callback(entry);
        }

        const uint64_t id = state.NextSubscriptionId++;
        state.Callbacks.emplace_back(id, std::move(callback));
        return LogSubscription(id);
    }

    void Log::Unsubscribe(const uint64_t id)
    {
        LogState& state = GetState();
        const std::lock_guard lock(state.Mutex);
        std::erase_if(state.Callbacks, [id](const auto& callback) { return callback.first == id; });
    }

    void LogSubscription::Reset()
    {
        if (m_Id == 0) return;

        Log::Unsubscribe(m_Id);
        m_Id = 0;
    }
}
