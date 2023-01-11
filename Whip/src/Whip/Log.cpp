#include <whippch.h>
#include <Whip/Log.h>

_WHIP_START

std::shared_ptr<spdlog::logger> Log::s_CoreLogger;
std::shared_ptr<spdlog::logger> Log::s_ClientLogger;

void Log::Init()
{
	spdlog::set_pattern("%^[%T] %n: %v%$");
	SET_LOG(s_CoreLogger, "WHIP ENGINE", trace);
	SET_LOG(s_ClientLogger, "Client", trace);
}

_WHIP_END