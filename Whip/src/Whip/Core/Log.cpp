#include "whippch.h"
#include "Log.h"

#pragma warning(push, 0)
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/async.h>
#pragma warning(pop)

_WHIP_START

std::shared_ptr<spdlog::logger> log::s_core_logger;
std::shared_ptr<spdlog::logger> log::s_client_logger;

std::shared_ptr<spdlog::logger> editor_log::s_editor_logger;
std::filesystem::path			editor_log::s_log_filepath;
bool							editor_log::s_should_log = true;
std::atomic<bool>				editor_log::s_file_should_reset{ false };

void log::init()
{
	spdlog::set_pattern("%^[%T] %n: %v%$");
	const spdlog::level::level_enum initial_level = log::whip_log_level_to_spdlog_level(level::trace);

	s_core_logger = spdlog::stdout_color_mt("WHIP ENGINE");
	s_core_logger->set_level(initial_level);

	s_client_logger = spdlog::stdout_color_mt("CLIENT");
	s_client_logger->set_level(initial_level);

	editor_log::init();
}

void log::reset_logger(logger& logger_in, const std::string& new_name, output_target target)
{
	if (new_name == s_core_logger->name() || new_name == s_client_logger->name() || new_name == editor_log::s_editor_logger->name())
		return;
	spdlog::level::level_enum level = spdlog::level::level_enum::trace;
	if (logger_in)
	{
		level = logger_in->level();
		spdlog::drop(logger_in->name());
	}
	if (target == output_target::editor)
	{
		std::string path = editor_log::s_log_filepath.string();
		logger_in = spdlog::rotating_logger_mt(new_name, path, editor_log::MAX_FILE_SIZE, editor_log::MAX_FILES);
		logger_in->flush_on(spdlog::level::trace);
		logger_in->set_pattern("level::%l,%^[%T] %n: %v%$");
	}
	else
	{
		logger_in = spdlog::stdout_color_mt(new_name);
	}
	logger_in->set_level(level);
}

spdlog::level::level_enum log::whip_log_level_to_spdlog_level(level log_level)
{
	switch (log_level)
	{
	case level::trace:			return spdlog::level::trace;
	case level::debug:			return spdlog::level::debug;
	case level::info:			return spdlog::level::info;
	case level::warn:			return spdlog::level::warn;
	case level::err:			return spdlog::level::err;
	case level::critical:		return spdlog::level::critical;
	case level::off:			return spdlog::level::off;
	case level::levels_size:	return spdlog::level::n_levels;
	default:					return spdlog::level::trace;
	}
}

void editor_log::init()
{
	std::filesystem::create_directory("log");
	s_log_filepath = std::filesystem::current_path() / "log/client.log";
	std::string path = s_log_filepath.string();
	s_editor_logger = spdlog::rotating_logger_mt("WHIP", path, MAX_FILE_SIZE, MAX_FILES - 1);
	s_editor_logger->set_level(spdlog::level::trace);
	s_editor_logger->flush_on(spdlog::level::trace);
	s_editor_logger->set_pattern("level::%l,%^[%T] %n: %v%$");
}

void editor_log::erase()
{
	spdlog::drop(s_editor_logger->name());
	s_editor_logger.reset();
}

WHP_NODISCARD log::level editor_log::whip_log_level_from_string(const std::string& level)
{
	if (level == "trace") return log::level::trace;
	if (level == "debug") return log::level::debug;
	if (level == "info") return log::level::info;
	if (level == "warning") return log::level::warn;
	if (level == "error") return log::level::err;
	if (level == "critical") return log::level::critical;

	return log::level::trace;
}

_WHIP_END
