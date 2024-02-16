#include "whippch.h"
#include "Log.h"

_WHIP_START

std::shared_ptr<spdlog::logger> log::s_core_logger;
std::shared_ptr<spdlog::logger> log::s_client_logger;

void log::init()
{
	spdlog::set_pattern("%^[%T] %n: %v%$");
	set_log(s_core_logger, "WHIP ENGINE", whip_log_level::trace);
	set_log(s_client_logger, "CLIENT", whip_log_level::trace);
}

inline void log::set_log(whp_logger& logger, const std::string& logger_name, whip_log_level log_level)
{
	logger = spdlog::stdout_color_mt(logger_name); 
	logger->set_level(_WHIP log::whip_log_level_to_spdlog_level(log_level));
}

// whip_log_level
spdlog::level::level_enum log::whip_log_level_to_spdlog_level(whip_log_level log_level)
{
	switch (log_level)
	{
	case whip_log_level::trace:			return spdlog::level::trace;
	case whip_log_level::debug:			return spdlog::level::debug;
	case whip_log_level::info:			return spdlog::level::info;
	case whip_log_level::warn:			return spdlog::level::warn;
	case whip_log_level::err:			return spdlog::level::err;
	case whip_log_level::critical:		return spdlog::level::critical;
	case whip_log_level::off:			return spdlog::level::off;
	case whip_log_level::levels_size:	return spdlog::level::n_levels;
	default:							return spdlog::level::off;
	}
}

_WHIP_END