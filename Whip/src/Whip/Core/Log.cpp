#include "whippch.h"
#include "Log.h"

_WHIP_START

std::shared_ptr<spdlog::logger> log::s_core_logger;
std::shared_ptr<spdlog::logger> log::s_client_logger;

void log::init()
{
	spdlog::set_pattern("%^[%T] %n: %v%$");
	SET_LOG(s_core_logger, "WHIP ENGINE", whp_trace);
	SET_LOG(s_client_logger, "Client", whp_trace);
}

spdlog::level::level_enum log::whip_log_level_to_spdlog_level(whip_log_level log_level)
{
	switch (log_level)
	{
	case whp_trace:			return spdlog::level::trace;
	case whp_debug:			return spdlog::level::debug;
	case whp_info:			return spdlog::level::info;
	case whp_warn:			return spdlog::level::warn;
	case whp_err:			return spdlog::level::err;
	case whp_critical:		return spdlog::level::critical;
	case whp_off:			return spdlog::level::off;
	case whp_levels_size:	return spdlog::level::n_levels;
	default:				return spdlog::level::off;
	}
}

_WHIP_END