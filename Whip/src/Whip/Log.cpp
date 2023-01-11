#include <whippch.h>
#include <Whip/Log.h>

_WHIP_START

std::shared_ptr<spdlog::logger> Log::s_CoreLogger;
std::shared_ptr<spdlog::logger> Log::s_ClientLogger;

void Log::Init()
{
	spdlog::set_pattern("%^[%T] %n: %v%$");
	s_CoreLogger = spdlog::stdout_color_mt("WHIP ENGINE");
	s_CoreLogger->set_level(spdlog::level::trace);

	s_ClientLogger = spdlog::stderr_color_mt("Client");
	s_ClientLogger->set_level(spdlog::level::trace);
}

_WHIP_END