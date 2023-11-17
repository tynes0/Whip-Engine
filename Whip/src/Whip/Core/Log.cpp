#include "whippch.h"
#include "Log.h"

_WHIP_START

std::shared_ptr<spdlog::logger> log::s_core_logger;
std::shared_ptr<spdlog::logger> log::s_client_logger;

void log::init()
{
	spdlog::set_pattern("%^[%T] %n: %v%$");
	SET_LOG(s_core_logger, "WHIP ENGINE", trace);
	SET_LOG(s_client_logger, "Client", trace);
}

spdlog::level::level_enum log::whip_log_level_to_spdlog_level(whip_log_level log_level)
{
	switch (log_level)
	{
	case trace:			return spdlog::level::trace;
	case debug:			return spdlog::level::debug;
	case info:			return spdlog::level::info;
	case warn:			return spdlog::level::warn;
	case err:			return spdlog::level::err;
	case critical:		return spdlog::level::critical;
	case off:			return spdlog::level::off;
	case levels_size:	return spdlog::level::n_levels;
	default:				return spdlog::level::off;
	}
}

_WHIP_END