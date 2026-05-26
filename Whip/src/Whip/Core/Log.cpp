#include "whippch.h"
#include "Log.h"

#pragma warning(push, 0)
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/async.h>
#pragma warning(pop)

#include <vector>

_WHIP_START

std::shared_ptr<spdlog::logger> log::s_core_logger;
std::shared_ptr<spdlog::logger> log::s_client_logger;

std::shared_ptr<spdlog::logger> editor_log::s_editor_logger;
spdlog::sink_ptr					editor_log::s_file_sink;
std::filesystem::path			editor_log::s_log_filepath;
bool							editor_log::s_should_log = true;
std::atomic<bool>				editor_log::s_file_should_reset{ false };

void log::init()
{
	const spdlog::level::level_enum initial_level = log::whip_log_level_to_spdlog_level(level::trace);

	editor_log::init();

	auto make_console_sink = []()
		{
			auto sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
			sink->set_pattern("%^[%T] %n: %v%$");
			return sink;
		};

	std::vector<spdlog::sink_ptr> core_sinks{ make_console_sink(), editor_log::s_file_sink };
	s_core_logger = std::make_shared<spdlog::logger>("WHIP ENGINE", core_sinks.begin(), core_sinks.end());
	s_core_logger->set_level(initial_level);
	s_core_logger->flush_on(spdlog::level::trace);
	spdlog::register_logger(s_core_logger);

	std::vector<spdlog::sink_ptr> client_sinks{ make_console_sink(), editor_log::s_file_sink };
	s_client_logger = std::make_shared<spdlog::logger>("CLIENT", client_sinks.begin(), client_sinks.end());
	s_client_logger->set_level(initial_level);
	s_client_logger->flush_on(spdlog::level::trace);
	spdlog::register_logger(s_client_logger);
}

void log::reset_logger(logger& logger_in, const std::string& new_name, output_target target)
{
	if ((s_core_logger && new_name == s_core_logger->name()) ||
		(s_client_logger && new_name == s_client_logger->name()) ||
		(editor_log::s_editor_logger && new_name == editor_log::s_editor_logger->name()))
		return;
	spdlog::level::level_enum level = spdlog::level::level_enum::trace;
	if (logger_in)
	{
		level = logger_in->level();
		spdlog::drop(logger_in->name());
	}
	if (target == output_target::editor)
	{
		logger_in = std::make_shared<spdlog::logger>(new_name, editor_log::s_file_sink);
		spdlog::register_logger(logger_in);
		logger_in->flush_on(spdlog::level::trace);
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
	case level::warning:		return spdlog::level::warn;
	case level::error:			return spdlog::level::err;
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
	s_file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(path, MAX_FILE_SIZE, MAX_FILES);
	s_file_sink->set_pattern("level::%l,[%T] %n: %v");
	s_editor_logger = std::make_shared<spdlog::logger>("WHIP", s_file_sink);
	spdlog::register_logger(s_editor_logger);
	s_editor_logger->set_level(spdlog::level::trace);
	s_editor_logger->flush_on(spdlog::level::trace);
}

void editor_log::erase()
{
	if (s_editor_logger)
		spdlog::drop(s_editor_logger->name());
	s_editor_logger.reset();
	s_file_sink.reset();
}

_WHIP_END
