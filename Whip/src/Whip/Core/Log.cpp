#include "whippch.h"
#include "Log.h"

_WHIP_START

std::shared_ptr<spdlog::logger> log::s_core_logger;
std::shared_ptr<spdlog::logger> log::s_client_logger;

void log::init()
{
	spdlog::set_pattern("%^[%T] %n: %v%$");
	set_log(s_core_logger, "WHIP ENGINE", log_level::trace);
	set_log(s_client_logger, "CLIENT", log_level::trace);
}

logger log::create_logger(const std::string& logger_name, log_level log_level)
{
	logger result;
	set_log(result, logger_name, log_level);
	return result;
}

void log::set_log(logger& logger, const std::string& logger_name, log_level log_level)
{
	logger = spdlog::stdout_color_mt(logger_name);
	logger->set_level(_WHIP log::whip_log_level_to_spdlog_level(log_level));
}

spdlog::level::level_enum log::whip_log_level_to_spdlog_level(log_level log_level)
{
	switch (log_level)
	{
	case log_level::trace:			return spdlog::level::trace;
	case log_level::debug:			return spdlog::level::debug;
	case log_level::info:			return spdlog::level::info;
	case log_level::warn:			return spdlog::level::warn;
	case log_level::err:			return spdlog::level::err;
	case log_level::critical:		return spdlog::level::critical;
	case log_level::off:			return spdlog::level::off;
	case log_level::levels_size:	return spdlog::level::n_levels;
	default:						return spdlog::level::off;
	}
}

_WHIP_END