#pragma once

#include <Whip/Core/Core.h>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/fmt/ostr.h>

_WHIP_START

typedef std::shared_ptr<spdlog::logger> whp_logger;

class WHIP_API log
{
private:
	static whp_logger s_core_logger;
	static whp_logger s_client_logger;
public:
	enum class whip_log_level : int
	{
		trace,
		debug,
		info,
		warn,
		err,
		critical,
		off,
		levels_size
	};
public:
	static void init();
	WHP_NODISCARD_MSG("Core logger returned as unnecessary") inline static const whp_logger& get_core_logger() { return s_core_logger; }
	WHP_NODISCARD_MSG("Client logger returned as unnecessary") inline static const whp_logger& get_client_logger() { return s_client_logger; }
	
	WHP_NODISCARD_MSG("Spdlog level returned as unnecessary") inline static spdlog::level::level_enum whip_log_level_to_spdlog_level(whip_log_level log_level);
};

_WHIP_END

// LOG SISTEMI KULLANIMI: Bu macro aktif olarak  

// Core log macros
#define WHP_CORE_TRACE(...)				_WHIP log::get_core_logger()->trace(__VA_ARGS__)
#define WHP_CORE_DEBUG(...)				_WHIP log::get_core_logger()->debug(__VA_ARGS__)
#define WHP_CORE_INFO(...)				_WHIP log::get_core_logger()->info(__VA_ARGS__)
#define WHP_CORE_WARN(...)				_WHIP log::get_core_logger()->warn(__VA_ARGS__)
#define WHP_CORE_ERROR(...)				_WHIP log::get_core_logger()->error(__VA_ARGS__)
#define WHP_CORE_CRITICAL(...)			_WHIP log::get_core_logger()->critical(__VA_ARGS__)

// Client log macros
#define WHP_CLIENT_TRACE(...)			_WHIP log::get_client_logger()->trace(__VA_ARGS__)
#define WHP_CLIENT_DEBUG(...)			_WHIP log::get_client_logger()->debug(__VA_ARGS__)
#define WHP_CLIENT_INFO(...)			_WHIP log::get_client_logger()->info(__VA_ARGS__)
#define WHP_CLIENT_WARN(...)			_WHIP log::get_client_logger()->warn(__VA_ARGS__)
#define WHP_CLIENT_ERROR(...)			_WHIP log::get_client_logger()->error(__VA_ARGS__)
#define WHP_CLIENT_CRITICAL(...)		_WHIP log::get_client_logger()->critical(__VA_ARGS__)

// SET_LOG macrosuna std::shared_ptr<spdlog::logger> türünde bir logger, std::string türünde bir logger ismi ve whip_log_level türünde bir log level verilir.Bu sayede bir logger üretmiþ oluruz. 
// set log macro
#define SET_LOG(logger, str_logger_name, logger_level)		logger = spdlog::stdout_color_mt(str_logger_name);\
															logger->set_level(_WHIP log::whip_log_level_to_spdlog_level(logger_level))
// WHP_LOG_ türevi macrolara bir adet logger ve format girilir (format detaylarý spdlog'da). Girilen logger girilen formatta bir log çýktýsý verir.
// Empty log macros
#define WHP_LOG_TRACE(logger, ...)		logger->trace(__VA_ARGS__)
#define WHP_LOG_DEBUG(logger, ...)		logger->debug(__VA_ARGS__)
#define WHP_LOG_INFO(logger, ...)		logger->info(__VA_ARGS__)
#define WHP_LOG_WARN(logger, ...)		logger->warn(__VA_ARGS__)
#define WHP_LOG_ERROR(logger, ...)		logger->error(__VA_ARGS__)
#define WHP_LOG_CRITICAL(logger, ...)	logger->critical(__VA_ARGS__)

// whip::log 
// init macro for simple usage
#define INIT_WHP_LOG _WHIP log::init()