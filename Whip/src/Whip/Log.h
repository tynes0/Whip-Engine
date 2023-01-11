#pragma once

#include <Whip/Core.h>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/fmt/ostr.h>

_WHIP_START

class WHIP_API Log
{
private:
	static std::shared_ptr<spdlog::logger> s_CoreLogger;
	static std::shared_ptr<spdlog::logger> s_ClientLogger;
public:
	static void Init();
	inline static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }
	inline static std::shared_ptr<spdlog::logger>& GetClientLogger() { return s_ClientLogger; }
};

_WHIP_END

// Core log macros
#define WHP_CORE_TRACE(...)		::Whip::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define WHP_CORE_INFO(...)		::Whip::Log::GetCoreLogger()->info(__VA_ARGS__)
#define WHP_CORE_WARN(...)		::Whip::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define WHP_CORE_ERROR(...)		::Whip::Log::GetCoreLogger()->error(__VA_ARGS__)
#define WHP_CORE_CRITICAL(...)	::Whip::Log::GetCoreLogger()->critical(__VA_ARGS__)

// Client log macros
#define WHP_TRACE(...)			::Whip::Log::GetClientLogger()->trace(__VA_ARGS__)
#define WHP_INFO(...)			::Whip::Log::GetClientLogger()->info(__VA_ARGS__)
#define WHP_WARN(...)			::Whip::Log::GetClientLogger()->warn(__VA_ARGS__)
#define WHP_ERROR(...)			::Whip::Log::GetClientLogger()->error(__VA_ARGS__)
#define WHP_CRITICAL(...)		::Whip::Log::GetClientLogger()->critical(__VA_ARGS__)

// set log macro
#define SET_LOG(logger, logger_name, logger_level)		logger = spdlog::stdout_color_mt(logger_name);\
														logger->set_level(spdlog::level::logger_level)

// init macro for simple usage
#define INIT_WHP_LOG ::Whip::Log::Init()