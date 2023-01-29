#pragma once

#include <Whip/Core.h>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/fmt/ostr.h>

_WHIP_START

typedef spdlog::logger whp_logger;
typedef std::shared_ptr<spdlog::logger> shared_whp_log;

class WHIP_API Log
{
private:
	static std::shared_ptr<spdlog::logger> s_core_logger;
	static std::shared_ptr<spdlog::logger> s_client_logger;
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
	static void init();
	WHP_NODISCARD_MSG("Core logger returned as unnecessary") inline static const std::shared_ptr<spdlog::logger>& get_core_logger() { return s_core_logger; }
	WHP_NODISCARD_MSG("Client logger returned as unnecessary") inline static const std::shared_ptr<spdlog::logger>& get_client_logger() { return s_client_logger; }
	
	WHP_NODISCARD_MSG("Spdlog level returned as unnecessary") inline static spdlog::level::level_enum whip_log_level_to_spdlog_level(whip_log_level log_level);
};

_WHIP_END

// Core log macros
#define WHP_CORE_TRACE(...)				::Whip::Log::get_core_logger()->trace(__VA_ARGS__)
#define WHP_CORE_DEBUG(...)				::Whip::Log::get_core_logger()->debug(__VA_ARGS__)
#define WHP_CORE_INFO(...)				::Whip::Log::get_core_logger()->info(__VA_ARGS__)
#define WHP_CORE_WARN(...)				::Whip::Log::get_core_logger()->warn(__VA_ARGS__)
#define WHP_CORE_ERROR(...)				::Whip::Log::get_core_logger()->error(__VA_ARGS__)
#define WHP_CORE_CRITICAL(...)			::Whip::Log::get_core_logger()->critical(__VA_ARGS__)

// Client log macros
#define WHP_CLIENT_TRACE(...)			::Whip::Log::get_client_logger()->trace(__VA_ARGS__)
#define WHP_CLIENT_DEBUG(...)			::Whip::Log::get_client_logger()->debug(__VA_ARGS__)
#define WHP_CLIENT_INFO(...)			::Whip::Log::get_client_logger()->info(__VA_ARGS__)
#define WHP_CLIENT_WARN(...)			::Whip::Log::get_client_logger()->warn(__VA_ARGS__)
#define WHP_CLIENT_ERROR(...)			::Whip::Log::get_client_logger()->error(__VA_ARGS__)
#define WHP_CLIENT_CRITICAL(...)		::Whip::Log::get_client_logger()->critical(__VA_ARGS__)

// Empty log macros
#define WHP_LOG_TRACE(logger, ...)		logger->trace(__VA_ARGS__)
#define WHP_LOG_DEBUG(logger, ...)		logger->trace(__VA_ARGS__)
#define WHP_LOG_INFO(logger, ...)		logger->trace(__VA_ARGS__)
#define WHP_LOG_WARN(logger, ...)		logger->trace(__VA_ARGS__)
#define WHP_LOG_ERROR(logger, ...)		logger->trace(__VA_ARGS__)
#define WHP_LOG_CRITICAL(logger, ...)	logger->trace(__VA_ARGS__)

// set log macro
#define SET_LOG(logger, str_logger_name, logger_level)		logger = spdlog::stdout_color_mt(str_logger_name);\
															logger->set_level(Log::whip_log_level_to_spdlog_level(logger_level))
// init macro for simple usage
#define INIT_WHP_LOG ::Whip::Log::init()