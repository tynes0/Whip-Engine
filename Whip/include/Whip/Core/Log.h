#pragma once

#include "Core.h"

#include <nps_formatter.h>

#pragma warning(push, 0)
#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>
#pragma warning(pop)

#include <filesystem>
#include <atomic>

#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/string_cast.hpp"

#include <frenum.h>

_WHIP_START

typedef std::shared_ptr<spdlog::logger> Logger;

class WHIP_API Log
{
public:
	enum class Level : int
	{
		Trace,
		Debug,
		Info,
		Warning,
		Error,
		Critical,
		Off,
		LevelsSize
	};

	enum class OutputTarget : int
	{
		Console,
		Editor
	};
public:
	static void Init();

	WHP_NODISCARD_MSG("Client Logger returned as unnecessary") inline static const Logger& GetClientLogger() { return s_ClientLogger; }
	WHP_NODISCARD_MSG("Core Logger returned as unnecessary") inline static const Logger& GetCoreLogger() { return s_CoreLogger; }
	
	static void ResetLogger(Logger& loggerIn, const std::string& newName, OutputTarget target = OutputTarget::Console);
	
	WHP_NODISCARD static spdlog::level::level_enum WhipLogLevelToSpdlogLevel(Level logLevel);
private:

	static Logger s_CoreLogger;
	static Logger s_ClientLogger;
};

template<typename OStream, glm::length_t L, typename T, glm::qualifier Q>
inline OStream& operator<<(OStream& os, const glm::vec<L, T, Q>& vector)
{
	return os << glm::to_string(vector);
}

template<typename OStream, glm::length_t C, glm::length_t R, typename T, glm::qualifier Q>
inline OStream& operator<<(OStream& os, const glm::mat<C, R, T, Q>& matrix)
{
	return os << glm::to_string(matrix);
}

template<typename OStream, typename T, glm::qualifier Q>
inline OStream& operator<<(OStream& os, glm::qua<T, Q> quaternion)
{
	return os << glm::to_string(quaternion);
}

class EditorLog
{
public:
	WHP_NODISCARD_MSG("Editor Logger returned as unnecessary") inline static const Logger& GetEditorLogger() { return s_EditorLogger; }

	static void LogState(bool shouldLog) { s_ShouldLog = shouldLog; }
	static bool ShouldLog() { return s_ShouldLog; }
	static std::atomic<bool>& FileShouldReset() { return s_FileShouldReset; }
	static void Erase();

	// Editor Logger
	template <class ...Args>
	inline static void Write(whip::Log::Level level, const std::string& message, Args&&... args)
	{
		if (!s_ShouldLog)
			return;
		s_EditorLogger->log(whip::Log::WhipLogLevelToSpdlogLevel(level), spdlog::fmt_lib::runtime(message), std::forward<Args>(args)...);
		s_FileShouldReset.store(true);
	}

	static constexpr size_t MaxFileSize = 5 * 1024 * 1024;
	static constexpr size_t MaxFiles = 1;

	static const std::filesystem::path& GetLogFilepath() { return s_LogFilepath; }
private:
	static void Init();

	static Logger s_EditorLogger;
	static spdlog::sink_ptr s_FileSink;
	static std::filesystem::path s_LogFilepath;
	static bool s_ShouldLog;
	static std::atomic<bool> s_FileShouldReset;

	friend class Log;
};

_WHIP_END

MakeFrenumWithNamespace(whip::Log, Level, Trace, Debug, Info, Warning, Error, Critical, Off, LevelsSize)

// Core log macros
#define WHP_CORE_TRACE(...)						whip::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define WHP_CORE_DEBUG(...)						whip::Log::GetCoreLogger()->debug(__VA_ARGS__)
#define WHP_CORE_INFO(...)						whip::Log::GetCoreLogger()->info(__VA_ARGS__)
#define WHP_CORE_WARN(...)						whip::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define WHP_CORE_ERROR(...)						whip::Log::GetCoreLogger()->error(__VA_ARGS__)
#define WHP_CORE_CRITICAL(...)					whip::Log::GetCoreLogger()->critical(__VA_ARGS__)

// Client log macros
#define WHP_CLIENT_TRACE(...)					whip::Log::GetClientLogger()->trace(__VA_ARGS__)
#define WHP_CLIENT_DEBUG(...)					whip::Log::GetClientLogger()->debug(__VA_ARGS__)
#define WHP_CLIENT_INFO(...)					whip::Log::GetClientLogger()->info(__VA_ARGS__)
#define WHP_CLIENT_WARN(...)					whip::Log::GetClientLogger()->warn(__VA_ARGS__)
#define WHP_CLIENT_ERROR(...)					whip::Log::GetClientLogger()->error(__VA_ARGS__)
#define WHP_CLIENT_CRITICAL(...)				whip::Log::GetClientLogger()->critical(__VA_ARGS__)

// Editor log macros
#define WHP_EDITOR_TRACE(message, ...)			whip::EditorLog::Write(whip::Log::Level::Trace ,message,__VA_ARGS__)
#define WHP_EDITOR_DEBUG(message,...)			whip::EditorLog::Write(whip::Log::Level::Debug ,message,__VA_ARGS__)
#define WHP_EDITOR_INFO(message,...)			whip::EditorLog::Write(whip::Log::Level::Info ,message,__VA_ARGS__)
#define WHP_EDITOR_WARN(message,...)			whip::EditorLog::Write(whip::Log::Level::Warning ,message,__VA_ARGS__)
#define WHP_EDITOR_ERROR(message,...)			whip::EditorLog::Write(whip::Log::Level::Error ,message,__VA_ARGS__)
#define WHP_EDITOR_CRITICAL(message,...)		whip::EditorLog::Write(whip::Log::Level::Critical ,message,__VA_ARGS__)


#ifdef WHP_ENABLE_ASSERTS
#define _WHP_INTERNAL_ASSERT_IMPL(type, check, msg, ...) do { if(!(check)) {WHP##type##CRITICAL(msg, __VA_ARGS__); WHP_DEBUGBREAK(); } } while(false)
#define _WHP_INTERNAL_ASSERT_WITH_MSG(type, check, ...)	_WHP_INTERNAL_ASSERT_IMPL(type, check, "Whip Assertion failed! File: {0}, Line: {1}, Message: {2}", std::filesystem::path(__FILE__).filename().string(), __LINE__ , __VA_ARGS__)
#define _WHP_INTERNAL_ASSERT_NO_MSG(type, check)			_WHP_INTERNAL_ASSERT_IMPL(type, check, "Whip Assertion '{0}' failed! File: {1}, Line: {2}", WHP_STRINGIZE(check), std::filesystem::path(__FILE__).filename().string(), __LINE__)

#define _WHP_INTERNAL_ASSERT_GET_MACRO_NAME(arg1, arg2, macro, ...) macro
#define _WHP_INTERNAL_ASSERT_GET_MACRO(...) WHP_EXPAND(_WHP_INTERNAL_ASSERT_GET_MACRO_NAME(__VA_ARGS__, _WHP_INTERNAL_ASSERT_WITH_MSG, _WHP_INTERNAL_ASSERT_NO_MSG))

#define WHP_ASSERT(...) WHP_EXPAND(_WHP_INTERNAL_ASSERT_GET_MACRO(__VA_ARGS__)(_CLIENT_, __VA_ARGS__))
#define WHP_CORE_ASSERT(...) WHP_EXPAND(_WHP_INTERNAL_ASSERT_GET_MACRO(__VA_ARGS__)(_CORE_, __VA_ARGS__))
#else //WHP_ENABLE_ASSERTS
#define WHP_ASSERT(x, ...)				// Whip assert not enabled
#define WHP_CORE_ASSERT(x, ...)			// Whip core assert not enabled
#endif //WHP_ENABLE_ASSERTS

#ifdef WHP_ENABLE_VERIFY
#define _WHP_INTERNAL_VERIFY_IMPL(type, check, msg, ...) { if(!(check)) {WHP##type##CRITICAL(msg, __VA_ARGS__); WHP_DEBUGBREAK(); } }
#define _WHP_INTERNAL_VERIFY_WITH_MSG(type, check, ...)	_WHP_INTERNAL_VERIFY_IMPL(type, check, "Whip verify failed! File: {0}, Line: {1}, Message: {2}", std::filesystem::path(__FILE__).filename().string(), __LINE__ , __VA_ARGS__)
#define _WHP_INTERNAL_VERIFY_NO_MSG(type, check)			_WHP_INTERNAL_VERIFY_IMPL(type, check, "Whip verify '{0}' failed! File: {1}, Line: {2}", WHP_STRINGIZE(check), std::filesystem::path(__FILE__).filename().string(), __LINE__)

#define _WHP_INTERNAL_VERIFY_GET_MACRO_NAME(arg1, arg2, macro, ...) macro
#define _WHP_INTERNAL_VERIFY_GET_MACRO(...) WHP_EXPAND(_WHP_INTERNAL_VERIFY_GET_MACRO_NAME(__VA_ARGS__, _WHP_INTERNAL_VERIFY_WITH_MSG, _WHP_INTERNAL_VERIFY_NO_MSG))

#define WHP_VERIFY(...) WHP_EXPAND(_WHP_INTERNAL_VERIFY_GET_MACRO(__VA_ARGS__)(_CLIENT_, __VA_ARGS__))
#define WHP_CORE_VERIFY(...) WHP_EXPAND(_WHP_INTERNAL_VERIFY_GET_MACRO(__VA_ARGS__)(_CORE_, __VA_ARGS__))
#else //WHP_ENABLE_VERIFY
#define WHP_VERIFY(x, ...)				// Whip verify not enabled
#define WHP_CORE_VERIFY(x, ...)			// Whip core verify not enabled
#endif //WHP_ENABLE_VERIFY
