#pragma once

#include <memory>

#include "core.h"

#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"

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
#define WH_CORE_TRACE(...)		::Whip::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define WH_CORE_INFO(...)		::Whip::Log::GetCoreLogger()->info(__VA_ARGS__)
#define WH_CORE_WARN(...)		::Whip::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define WH_CORE_ERROR(...)		::Whip::Log::GetCoreLogger()->error(__VA_ARGS__)
//#define WH_CORE_FATAL(...)		::Whip::Log::GetCoreLogger()->fatal(__VA_ARGS__)

#define WH_TRACE(...)		::Whip::Log::GetClientLogger()->trace(__VA_ARGS__)
#define WH_INFO(...)		::Whip::Log::GetClientLogger()->info(__VA_ARGS__)
#define WH_WARN(...)		::Whip::Log::GetClientLogger()->warn(__VA_ARGS__)
#define WH_ERROR(...)		::Whip::Log::GetClientLogger()->error(__VA_ARGS__)
//#define WH_FATAL(...)		::Whip::Log::GetClientLogger()->fatal(__VA_ARGS__)


