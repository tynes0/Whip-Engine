#pragma once

#include <Whip/Core.h>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/fmt/ostr.h>

_WHIP_START

class WHIP_API Log
{
private:
	static std::shared_ptr<spdlog::logger> s_CoreLogger;
	static std::shared_ptr<spdlog::logger> s_ClientLogger;
public:
	enum whip_log_level : int
	{
		whp_trace,
		whp_debug,
		whp_info,
		whp_warn,
		whp_err,
		whp_critical,
		whp_off,
		whp_levels_size
	};
public:
	static void Init();
	WHP_NODISCARD inline static const std::shared_ptr<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }
	WHP_NODISCARD inline static const std::shared_ptr<spdlog::logger>& GetClientLogger() { return s_ClientLogger; }
	
	WHP_NODISCARD inline static spdlog::level::level_enum whip_log_level_to_spdlog_level(whip_log_level log_level);
};


_WHIP_END

// Core log macros
#define WHP_CORE_TRACE(...)		WHP_LOG GetCoreLogger()->trace(__VA_ARGS__)
#define WHP_CORE_DEBUGL(...)	WHP_LOG GetCoreLogger()->debug(__VA_ARGS__)
#define WHP_CORE_INFO(...)		WHP_LOG GetCoreLogger()->info(__VA_ARGS__)
#define WHP_CORE_WARN(...)		WHP_LOG GetCoreLogger()->warn(__VA_ARGS__)
#define WHP_CORE_ERROR(...)		WHP_LOG GetCoreLogger()->error(__VA_ARGS__)
#define WHP_CORE_CRITICAL(...)	WHP_LOG GetCoreLogger()->critical(__VA_ARGS__)

// Client log macros
#define WHP_TRACE(...)			WHP_LOG GetClientLogger()->trace(__VA_ARGS__)
#define WHP_DEBUGL(...)			WHP_LOG GetClientLogger()->debug(__VA_ARGS__)
#define WHP_INFO(...)			WHP_LOG GetClientLogger()->info(__VA_ARGS__)
#define WHP_WARN(...)			WHP_LOG GetClientLogger()->warn(__VA_ARGS__)
#define WHP_ERROR(...)			WHP_LOG GetClientLogger()->error(__VA_ARGS__)
#define WHP_CRITICAL(...)		WHP_LOG GetClientLogger()->critical(__VA_ARGS__)

// set log macro
#define SET_LOG(logger, str_logger_name, logger_level)		logger = spdlog::stdout_color_mt(str_logger_name);\
															logger->set_level(Log::whip_log_level_to_spdlog_level(logger_level))

// init macro for simple usage
#define INIT_WHP_LOG ::Whip::Log::Init()