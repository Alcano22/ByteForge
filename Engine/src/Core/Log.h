#pragma once

#include "Base.h"

#pragma warning(push, 0)
#include <spdlog/spdlog.h>
#pragma warning(pop)

class Log
{
public:
	static void Init();

	static Ref<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }
	static Ref<spdlog::logger>& GetClientLogger() { return s_ClientLogger; }
private:
	static Ref<spdlog::logger> s_CoreLogger;
	static Ref<spdlog::logger> s_ClientLogger;
};

#define BF_LOG_CORE_TRACE(...)		::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define BF_LOG_CORE_DEBUG(...)		::Log::GetCoreLogger()->debug(__VA_ARGS__)
#define BF_LOG_CORE_INFO(...)		::Log::GetCoreLogger()->info(__VA_ARGS__)
#define BF_LOG_CORE_WARN(...)		::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define BF_LOG_CORE_ERROR(...)		::Log::GetCoreLogger()->error(__VA_ARGS__)
#define BF_LOG_CORE_FATAL(...)		::Log::GetCoreLogger()->critical(__VA_ARGS__)

#define BF_LOG_TRACE(...)   		::Log::GetClientLogger()->trace(__VA_ARGS__)
#define BF_LOG_DEBUG(...)   		::Log::GetClientLogger()->debug(__VA_ARGS__)
#define BF_LOG_INFO(...)   			::Log::GetClientLogger()->info(__VA_ARGS__)
#define BF_LOG_WARN(...)   			::Log::GetClientLogger()->warn(__VA_ARGS__)
#define BF_LOG_ERROR(...)   		::Log::GetClientLogger()->error(__VA_ARGS__)
#define BF_LOG_FATAL(...)   		::Log::GetClientLogger()->critical(__VA_ARGS__)
