#pragma once

#include "Core.h"

#include "../vendor/nps/nps_formatter.h"

#pragma warning(push, 0)
#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>
#pragma warning(pop)

#include <filesystem>
#include <atomic>

#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/string_cast.hpp"

#include "../frenum/frenum.h"

_WHIP_START

typedef std::shared_ptr<spdlog::logger> logger;

class WHIP_API log
{
public:
	enum class level : int
	{
		trace,
		debug,
		info,
		warning,
		error,
		critical,
		off,
		levels_size
	};

	enum class output_target : int
	{
		console,
		editor
	};
public:
	static void init();

	WHP_NODISCARD_MSG("Client logger returned as unnecessary") inline static const logger& get_client_logger() { return s_client_logger; }
	WHP_NODISCARD_MSG("Core logger returned as unnecessary") inline static const logger& get_core_logger() { return s_core_logger; }
	
	static void reset_logger(logger& logger_in, const std::string& new_name, output_target target = output_target::console);
	
	WHP_NODISCARD static spdlog::level::level_enum whip_log_level_to_spdlog_level(level log_level);
private:

	static logger s_core_logger;
	static logger s_client_logger;
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

class editor_log
{
public:
	WHP_NODISCARD_MSG("Editor logger returned as unnecessary") inline static const logger& get_editor_logger() { return s_editor_logger; }

	static void log_state(bool should_log) { s_should_log = should_log; }
	static bool should_log() { return s_should_log; }
	static std::atomic<bool>& file_should_reset() { return s_file_should_reset; }
	static void erase();

	// editor logger
	template <class ...Args>
	inline static void log(log::level level, const std::string& message, Args&&... args)
	{
		if (!s_should_log)
			return;
		s_editor_logger->log(log::whip_log_level_to_spdlog_level(level),message, std::forward<Args>(args)...);
		s_file_should_reset.store(true);
	}

	static constexpr size_t MAX_FILE_SIZE = 5 * 1024 * 1024;
	static constexpr size_t MAX_FILES = 1;

	static const std::filesystem::path& get_log_filepath() { return s_log_filepath; }
private:
	static void init();

	static logger s_editor_logger;
	static std::filesystem::path s_log_filepath;
	static bool s_should_log;
	static std::atomic<bool> s_file_should_reset;

	friend class log;
};

_WHIP_END

MakeFrenumWithNamespace(whip::log, level, trace, debug, info, warning, error, critical, off, levels_size)

// Core log macros
#define WHP_CORE_TRACE(...)						whip::log::get_core_logger()->trace(__VA_ARGS__)
#define WHP_CORE_DEBUG(...)						whip::log::get_core_logger()->debug(__VA_ARGS__)
#define WHP_CORE_INFO(...)						whip::log::get_core_logger()->info(__VA_ARGS__)
#define WHP_CORE_WARN(...)						whip::log::get_core_logger()->warn(__VA_ARGS__)
#define WHP_CORE_ERROR(...)						whip::log::get_core_logger()->error(__VA_ARGS__)
#define WHP_CORE_CRITICAL(...)					whip::log::get_core_logger()->critical(__VA_ARGS__)

// Client log macros
#define WHP_CLIENT_TRACE(...)					whip::log::get_client_logger()->trace(__VA_ARGS__)
#define WHP_CLIENT_DEBUG(...)					whip::log::get_client_logger()->debug(__VA_ARGS__)
#define WHP_CLIENT_INFO(...)					whip::log::get_client_logger()->info(__VA_ARGS__)
#define WHP_CLIENT_WARN(...)					whip::log::get_client_logger()->warn(__VA_ARGS__)
#define WHP_CLIENT_ERROR(...)					whip::log::get_client_logger()->error(__VA_ARGS__)
#define WHP_CLIENT_CRITICAL(...)				whip::log::get_client_logger()->critical(__VA_ARGS__)

// Editor log macros
#define WHP_EDITOR_TRACE(message, ...)			whip::editor_log::log(whip::log::level::trace ,message,__VA_ARGS__)
#define WHP_EDITOR_DEBUG(message,...)			whip::editor_log::log(whip::log::level::debug ,message,__VA_ARGS__)
#define WHP_EDITOR_INFO(message,...)			whip::editor_log::log(whip::log::level::info ,message,__VA_ARGS__)
#define WHP_EDITOR_WARN(message,...)			whip::editor_log::log(whip::log::level::warning ,message,__VA_ARGS__)
#define WHP_EDITOR_ERROR(message,...)			whip::editor_log::log(whip::log::level::error ,message,__VA_ARGS__)
#define WHP_EDITOR_CRITICAL(message,...)		whip::editor_log::log(whip::log::level::critical ,message,__VA_ARGS__)


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
