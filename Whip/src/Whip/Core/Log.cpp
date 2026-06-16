#include "WhipPch.h"
#include <Whip/Core/Log.h>

#pragma warning(push, 0)
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/async.h>
#pragma warning(pop)

#include <vector>

_WHIP_START

std::shared_ptr<spdlog::logger> Log::s_CoreLogger;
std::shared_ptr<spdlog::logger> Log::s_ClientLogger;

std::shared_ptr<spdlog::logger> EditorLog::s_EditorLogger;
spdlog::sink_ptr					EditorLog::s_FileSink;
std::filesystem::path			EditorLog::s_LogFilepath;
bool							EditorLog::s_ShouldLog = true;
std::atomic<bool>				EditorLog::s_FileShouldReset{ false };

void Log::Init()
{
	const spdlog::level::level_enum initialLevel = Log::WhipLogLevelToSpdlogLevel(Level::Trace);

	EditorLog::Init();

	auto makeConsoleSink = []()
		{
			auto sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
			sink->set_pattern("%^[%T] %n: %v%$");
			return sink;
		};

	std::vector<spdlog::sink_ptr> coreSinks{ makeConsoleSink(), EditorLog::s_FileSink };
	s_CoreLogger = std::make_shared<spdlog::logger>("WHIP ENGINE", coreSinks.begin(), coreSinks.end());
	s_CoreLogger->set_level(initialLevel);
	s_CoreLogger->flush_on(spdlog::level::trace);
	spdlog::register_logger(s_CoreLogger);

	std::vector<spdlog::sink_ptr> clientSinks{ makeConsoleSink(), EditorLog::s_FileSink };
	s_ClientLogger = std::make_shared<spdlog::logger>("CLIENT", clientSinks.begin(), clientSinks.end());
	s_ClientLogger->set_level(initialLevel);
	s_ClientLogger->flush_on(spdlog::level::trace);
	spdlog::register_logger(s_ClientLogger);
}

void Log::ResetLogger(Logger& loggerIn, const std::string& newName, OutputTarget target)
{
	if ((s_CoreLogger && newName == s_CoreLogger->name()) ||
		(s_ClientLogger && newName == s_ClientLogger->name()) ||
		(EditorLog::s_EditorLogger && newName == EditorLog::s_EditorLogger->name()))
		return;
	spdlog::level::level_enum level = spdlog::level::level_enum::trace;
	if (loggerIn)
	{
		level = loggerIn->level();
		spdlog::drop(loggerIn->name());
	}
	if (target == OutputTarget::Editor)
	{
		loggerIn = std::make_shared<spdlog::logger>(newName, EditorLog::s_FileSink);
		spdlog::register_logger(loggerIn);
		loggerIn->flush_on(spdlog::level::trace);
	}
	else
	{
		loggerIn = spdlog::stdout_color_mt(newName);
	}
	loggerIn->set_level(level);
}

spdlog::level::level_enum Log::WhipLogLevelToSpdlogLevel(Level logLevel)
{
	switch (logLevel)
	{
	case Level::Trace:			return spdlog::level::trace;
	case Level::Debug:			return spdlog::level::debug;
	case Level::Info:			return spdlog::level::info;
	case Level::Warning:		return spdlog::level::warn;
	case Level::Error:			return spdlog::level::err;
	case Level::Critical:		return spdlog::level::critical;
	case Level::Off:			return spdlog::level::off;
	case Level::LevelsSize:	return spdlog::level::n_levels;
	default:					return spdlog::level::trace;
	}
}

void EditorLog::Init()
{
	std::filesystem::create_directory("log");
	s_LogFilepath = std::filesystem::current_path() / "log/client.log";
	std::string path = s_LogFilepath.string();
	s_FileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(path, MaxFileSize, MaxFiles);
	s_FileSink->set_pattern("level::%l,[%T] %n: %v");
	s_EditorLogger = std::make_shared<spdlog::logger>("WHIP", s_FileSink);
	spdlog::register_logger(s_EditorLogger);
	s_EditorLogger->set_level(spdlog::level::trace);
	s_EditorLogger->flush_on(spdlog::level::trace);
}

void EditorLog::Erase()
{
	if (s_EditorLogger)
		spdlog::drop(s_EditorLogger->name());
	s_EditorLogger.reset();
	s_FileSink.reset();
}

_WHIP_END
