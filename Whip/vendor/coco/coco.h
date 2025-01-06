/*
 * This file is part of the Coco library, originally created by Tynes0.
 * For the latest version and updates, please visit the official Coco GitHub repository:
 * https://github.com/tynes0/coco
 *
 * Coco is a comprehensive library for time measurements, providing JSON generation for use in the tracing part of internet browsers, logging data to a given file, logging data to the console, etc..
 * It is released under the Apache License 2.0 license. See the LICENSE file in the root of the Coco repository
 * or visit the above GitHub link for more details.
 *
 * For contributions, bug reports, or other inquiries, feel free to contact the author:
 * - GitHub: https://github.com/tynes0
 * - Email: cihanbilgihan@gmail.com
 */

#pragma once

#include <chrono>
#include <thread>
#include <iostream>
#include <fstream>
#include <numeric>
#include <cassert>
#include <unordered_map>
#include <filesystem>
#include <algorithm>

namespace sch = std::chrono;

#ifndef COCO_ASSERT
#ifdef _DEBUG
#define COCO_ASSERT(condition, message)																\
    do																								\
    {																								\
        if (!(condition))																			\
        {																							\
            std::cerr << "Assertion failed: " << #condition << " (" << message << ")" << std::endl; \
            std::cerr << "File: " << __FILE__ << ", Line: " << __LINE__ << std::endl;				\
            std::abort();																			\
        }																							\
    } while (false)
#else //_DEBUG
#define COCO_ASSERT(condition, message)
#endif // _DEBUG
#endif // !COCO_ASSERT

#if _HAS_CXX17
#define COCO_INLINE inline
#else// _HAS_CXX17
#define COCO_INLINE 
#endif // _HAS_CXX17


namespace coco
{
	using clock_t = sch::high_resolution_clock;
	namespace time_units
	{
		struct nanoseconds
		{
			using type = sch::nanoseconds;
			static constexpr const char* name = "nanoseconds";
		};

		struct microseconds
		{
			using type = sch::microseconds;
			static constexpr const char* name = "microseconds";
		};

		struct milliseconds
		{
			using type = sch::milliseconds;
			static constexpr const char* name = "milliseconds";
		};

		struct seconds
		{
			using type = sch::seconds;
			static constexpr const char* name = "seconds";
		};

		struct minutes
		{
			using type = sch::minutes;
			static constexpr const char* name = "minutes";
		};

		struct hours
		{
			using type = sch::hours;
			static constexpr const char* name = "hours";
		};
	}

#ifdef __cpp_concepts
	namespace detail
	{
		template <class _Duration>
		concept duration_type = std::_Is_any_of_v<_Duration, coco::time_units::nanoseconds, coco::time_units::microseconds, coco::time_units::milliseconds, coco::time_units::seconds, coco::time_units::minutes, coco::time_units::hours>;
	}

#define _COCO_ENABLE_IF_DURATION_T(dur)
#define _COCO_CONCEPT_DURATION_T detail::duration_type
#else // __cpp_concepts
#define _COCO_ENABLE_IF_DURATION_T(dur) , std::enable_if_t<std::_Is_any_of_v<dur, coco::time_units::nanoseconds, coco::time_units::microseconds, coco::time_units::milliseconds, coco::time_units::seconds, coco::time_units::minutes, coco::time_units::hours>, int> = 0
#define _COCO_CONCEPT_DURATION_T class
#endif // __cpp_concepts

	template <_COCO_CONCEPT_DURATION_T _From, _COCO_CONCEPT_DURATION_T _To _COCO_ENABLE_IF_DURATION_T(_From) _COCO_ENABLE_IF_DURATION_T(_To)>
	long long duration_count_cast(long long value)
	{
		typename _From::type from_dur(value);
		return sch::duration_cast<typename _To::type>(from_dur).count();
	}

	namespace detail
	{
		struct profile_result
		{
			std::string name;
			long long start, end;
			size_t threadID;
		};

		struct instrumentation_session
		{
			std::string m_name;
		};
	}

	class instrumentor
	{
	public:
		instrumentor() : m_current_session(nullptr), m_profile_count(0), m_active(false) {}

		void begin_session(const std::string& name, const std::string& filepath = "results.json")
		{
			if (!m_active)
			{
				m_output_stream.open(filepath);
				write_header();
				m_current_session = new detail::instrumentation_session{ name };
				m_active = true;
			}
		}

		void end_session()
		{
			if (m_active)
			{
				write_footer();
				m_output_stream.close();
				delete m_current_session;
				m_current_session = nullptr;
				m_profile_count = 0;
				m_active = false;
			}
		}

		void write_profile(const detail::profile_result& result)
		{
			COCO_ASSERT(m_active, "write_profile() called on inactive instrumentor");
			if (m_active)
			{
				if (m_profile_count++ > 0)
					m_output_stream << ",";

				std::string name = result.name;
				std::replace(name.begin(), name.end(), '"', '\'');

				m_output_stream << "{\"cat\":\"function\",";
				m_output_stream << "\"dur\":" << (result.end - result.start) << ',';
				m_output_stream << "\"name\":\"" << name << "\",";
				m_output_stream << "\"ph\":\"X\",";
				m_output_stream << "\"pid\":0,";
				m_output_stream << "\"tid\":" << result.threadID << ",";
				m_output_stream << "\"ts\":" << result.start << "}";

				m_output_stream.flush();
			}
		}
	private:
		void write_header()
		{
			m_output_stream << "{\"otherData\": {},\"traceEvents\":[";
			m_output_stream.flush();
		}

		void write_footer()
		{
			m_output_stream << "]}";
			m_output_stream.flush();
		}
	public:
		static instrumentor& get()
		{
			static instrumentor instance;
			return instance;
		}

	private:
		detail::instrumentation_session* m_current_session;
		std::ofstream m_output_stream;
		int m_profile_count;
		bool m_active;
	};

	struct dont_start {};

	template <_COCO_CONCEPT_DURATION_T _Duration = coco::time_units::microseconds _COCO_ENABLE_IF_DURATION_T(_Duration)>
	class timer
	{
	public:
		timer(const std::string& name = std::string{ "Coco Timer" }, bool print_state = false) : m_name(name), m_print_when_stopped(print_state)
		{
			start();
		}

		timer(dont_start) : m_name(std::string{ "Coco Timer" }), m_print_when_stopped(false)
		{
			m_time = 0;
			m_paused = false;
			m_stopped = true;
		}

		~timer()
		{
			stop();
		}

		void start()
		{
			if (m_stopped)
			{
				m_time = 0;
				m_paused = false;
				m_stopped = false;
				m_timepoint = now();
			}
		}

		void pause()
		{
			COCO_ASSERT(!m_stopped, "pause() called on inactive timer.");
			if (!m_stopped && !m_paused)
			{
				m_paused = true;
				long long start = tp_cast(m_timepoint).time_since_epoch().count();
				long long end = tp_cast(now()).time_since_epoch().count();
				m_time += (end - start);
			}
		}

		void resume()
		{
			if (m_paused)
			{
				m_paused = false;
				m_timepoint = now();
			}
		}

		void reset()
		{
			m_time = 0;
			m_paused = false;
			m_stopped = false;
			m_timepoint = now();
		}

		void stop()
		{
			if (!m_stopped)
			{
				m_stopped = true;
				long long start = tp_cast(m_timepoint).time_since_epoch().count();
				long long end = tp_cast(now()).time_since_epoch().count();

				m_time += (end - start);
				if (m_print_when_stopped)
					std::cout << m_name << " : " << m_time << ' ' << _Duration::name << "\n";
			}
		}

		bool complated_on_time(long long time) const noexcept
		{
			if (!m_stopped)
				return false;
			return time >= m_time;
		}

		bool is_running() const noexcept
		{
			return !m_stopped;
		}

		bool is_paused() const noexcept
		{
			return m_paused;
		}

		void set_print_state(bool state)
		{
			m_print_when_stopped = state;
		}

		bool is_printing() const noexcept
		{
			return m_print_when_stopped;
		}

		long long get_time() const
		{
			return m_time;
		}

		template <_COCO_CONCEPT_DURATION_T _To _COCO_ENABLE_IF_DURATION_T(_To)>
		long long get_casted_time() const
		{
			return duration_count_cast<_Duration, _To>(m_time);
		}

	private:
		sch::time_point<clock_t> now()
		{
			return clock_t::now();
		}

		constexpr sch::time_point<clock_t, typename _Duration::type> tp_cast(const sch::time_point<clock_t>& tp)
		{
			return sch::time_point_cast<typename _Duration::type>(tp);
		}

		sch::time_point<clock_t> m_timepoint;
		std::string m_name;
		bool m_print_when_stopped;
		long long m_time = 0;
		bool m_stopped = true;
		bool m_paused = false;
	};

	template <typename T>
	struct is_timer : std::false_type {};

	template <typename _Duration>
	struct is_timer<timer<_Duration>> : std::true_type {};

	template <typename T>
	COCO_INLINE constexpr bool is_timer_v = is_timer<T>::value;

#ifdef __cpp_concepts
	namespace detail
	{
		template <class _Duration>
		concept timer_type = is_timer_v<_Duration>;
	}

#define _COCO_ENABLE_IF_TIMER_T(_Timer)
#define _COCO_CONCEPT_TIMER_T detail::timer_type
#else // __cpp_concepts
#define _COCO_ENABLE_IF_TIMER_T(_Timer) , std::enable_if_t<is_timer_v<_Timer>, int> = 0
#define _COCO_CONCEPT_TIMER_T class
#endif // __cpp_concepts

	class instrumentation_timer
	{
	public:
		instrumentation_timer(const std::string& name) : m_name(name)
		{
			start();
		}

		instrumentation_timer(const std::string& name, dont_start) : m_name(name)
		{
			m_time = 0;
			m_stopped = true;
		}

		~instrumentation_timer()
		{
			stop();
		}

		void start()
		{
			m_time = 0;
			m_stopped = false;
			m_timepoint = now();
		}

		void stop()
		{
			if (!m_stopped)
			{
				auto end_timepoint = clock_t::now();

				long long start = tp_cast(m_timepoint).time_since_epoch().count();
				long long end = tp_cast(end_timepoint).time_since_epoch().count();

				m_time = end - start;

				size_t threadID = std::hash<std::thread::id>{}(std::this_thread::get_id());
				instrumentor::get().write_profile({ m_name, start, end, threadID });
				m_stopped = true;
			}
		}

		bool complated_on_time(long long time) const noexcept
		{
			if (!m_stopped)
				return false;
			return time >= m_time;
		}

		long long get_time() const
		{
			return m_time;
		}

		template <_COCO_CONCEPT_DURATION_T _To _COCO_ENABLE_IF_DURATION_T(_To)>
		long long get_casted_time() const
		{
			return duration_count_cast<time_units::milliseconds, _To>(m_time);
		}

	private:
		sch::time_point<clock_t> now()
		{
			return clock_t::now();
		}

		constexpr sch::time_point<clock_t, typename time_units::milliseconds::type> tp_cast(const sch::time_point<clock_t>& tp)
		{
			return sch::time_point_cast<typename time_units::milliseconds::type>(tp);
		}

		sch::time_point<clock_t> m_timepoint;
		std::string m_name;
		long long m_time = 0;
		bool m_stopped = false;
	};

	class timer_statistics
	{
	public:
		void add_measurement(long long time)
		{
			m_measurements.push_back(time);
		}

		void clear_measurements()
		{
			m_measurements.clear();
		}

		double calculate_average() const
		{
			if (m_measurements.empty())
			{
				COCO_ASSERT(false, "no measurements found");
				return 0.0;
			}
			long long sum = std::accumulate(m_measurements.begin(), m_measurements.end(), 0LL);
			return static_cast<double>(sum) / m_measurements.size();
		}

		double calculate_variance() const
		{
			if (m_measurements.empty())
			{
				COCO_ASSERT(false, "no measurements found");
				return 0.0;
			}
			double avg = calculate_average();
			double variance = 0.0;
			for (long long time : m_measurements)
				variance += std::pow(time - avg, 2);
			variance /= m_measurements.size();
			return variance;
		}

		double calculate_standard_deviation() const
		{
			return std::sqrt(calculate_variance());
		}

		double calculate_median() const
		{
			if (m_measurements.empty())
			{
				COCO_ASSERT(false, "no measurements found");
				return 0.0;
			}
			std::vector<long long> sorted_measurements = m_measurements;
			std::sort(sorted_measurements.begin(), sorted_measurements.end());
			size_t n = sorted_measurements.size();
			if (n % 2 == 0)
				return static_cast<double>(sorted_measurements[n / 2 - 1] + sorted_measurements[n / 2]) / 2.0;
			else
				return static_cast<double>(sorted_measurements[n / 2]);
		}

		long long get_min_value() const
		{
			if (m_measurements.empty())
			{
				COCO_ASSERT(false, "no measurements found");
				return 0;
			}
			return *std::min_element(m_measurements.begin(), m_measurements.end());
		}

		long long get_max_value() const
		{
			if (m_measurements.empty())
			{
				COCO_ASSERT(false, "no measurements found");
				return 0;
			}
			return *std::max_element(m_measurements.begin(), m_measurements.end());
		}

		size_t get_measurement_count() const
		{
			return m_measurements.size();
		}

	private:
		std::vector<long long> m_measurements;
	};

	class timer_data_logger
	{
	public:
		timer_data_logger() : m_stats(std::make_shared<timer_statistics>()) {}

		timer_data_logger(const timer_statistics& stats) : m_stats(std::make_shared<timer_statistics>(stats)) {}

		timer_data_logger(timer_statistics&& stats) noexcept : m_stats(std::make_shared<timer_statistics>(std::move(stats))) {}

		~timer_data_logger() {}

		std::shared_ptr<timer_statistics> get_statistics() const
		{
			return m_stats;
		}

		timer_data_logger& operator=(const timer_data_logger& other)
		{
			m_stats = other.m_stats;
			return *this;
		}

		timer_data_logger& operator=(timer_data_logger&& other) noexcept
		{
			m_stats = std::move(other.m_stats);
			return *this;
		}

		void add_measurement(long long time)
		{
			m_stats->add_measurement(time);
		}

		void set_log_path(const std::filesystem::path& filepath)
		{
			m_log_path = filepath;
		}

		template <_COCO_CONCEPT_DURATION_T _Duration = coco::time_units::microseconds _COCO_ENABLE_IF_DURATION_T(_Duration)>
		void log_statistics() const
		{
			if (m_log_path.empty())
				return;
			log_statistics(m_log_path);
		}

		template <_COCO_CONCEPT_DURATION_T _Duration = coco::time_units::microseconds _COCO_ENABLE_IF_DURATION_T(_Duration)>
		void log_statistics(const std::filesystem::path& filepath) const
		{
			COCO_ASSERT(!filepath.empty(), "Log path is empty!");
			std::ofstream file(filepath);
			if (file.is_open())
			{
				file << "Statistics Summary:\n";
				file << "-------------------\n";
				file << "Number of attempts: " << m_stats->get_measurement_count() << " times\n";
				file << "Average Time: " << m_stats->calculate_average() << ' ' << _Duration::name << "\n";
				file << "Variance: " << m_stats->calculate_variance() << ' ' << _Duration::name << "\n";
				file << "Standard Deviation: " << m_stats->calculate_standard_deviation() << ' ' << _Duration::name << "\n";
				file << "Median Time: " << m_stats->calculate_median() << ' ' << _Duration::name << "\n";
				file << "Minimum Time: " << m_stats->get_min_value() << ' ' << _Duration::name << "\n";
				file << "Maximum Time: " << m_stats->get_max_value() << ' ' << _Duration::name << "\n";
				file << "-------------------\n";
				file.close();
			}
			else
			{
				COCO_ASSERT(false, "Failed to open file for writing.");
			}
		}

	private:
		std::shared_ptr<timer_statistics> m_stats;
		std::filesystem::path m_log_path;
	};

	template <_COCO_CONCEPT_DURATION_T _Duration = coco::time_units::microseconds _COCO_ENABLE_IF_DURATION_T(_Duration)>
	class multiple_timer_manager
	{
	public:
		multiple_timer_manager() {}

		~multiple_timer_manager() {}

		void add(std::string_view timer_name)
		{
			if (m_timers.find(timer_name) == m_timers.end())
				m_timers[timer_name] = std::make_shared<coco::timer<_Duration>>(dont_start{});
			COCO_ASSERT(false, "Timer already exists!");
		}

		void remove(std::string_view timer_name)
		{
			auto it = m_timers.find(timer_name);
			if (it != m_timers.end())
				m_timers.erase(it);
			COCO_ASSERT(false, "Timer not found!");
		}

		std::shared_ptr<coco::timer<_Duration>> get(std::string_view timer_name)
		{
			auto it = m_timers.find(timer_name);
			if (it != m_timers.end())
				return m_timers.at(timer_name);
			COCO_ASSERT(false, "Timer not found");
			return nullptr;
		}

		const std::shared_ptr<coco::timer<_Duration>> get(std::string_view timer_name) const
		{
			auto it = m_timers.find(timer_name);
			if (it != m_timers.end())
				return m_timers.at(timer_name);
			COCO_ASSERT(false, "Timer not found");
			return nullptr;
		}

		std::shared_ptr<coco::timer<_Duration>> operator[](std::string_view timer_name)
		{
			return get(timer_name);
		}

		const std::shared_ptr<coco::timer<_Duration>> operator[](std::string_view timer_name) const
		{
			return get(timer_name);
		}

		void reset_all()
		{
			for (auto& timer : m_timers)
				timer.second->reset();
		}

		void stop_all()
		{
			for (auto& timer : m_timers)
				timer.second->stop();
		}
	private:
		std::unordered_map<std::string_view, std::shared_ptr<coco::timer<_Duration>>> m_timers;
	};

	class multiple_timer_data_logger
	{
	public:
		multiple_timer_data_logger() {}

		~multiple_timer_data_logger() {}

		void add(std::string_view logger_name)
		{
			if (m_loggers.find(logger_name) == m_loggers.end())
				m_loggers[logger_name] = std::make_shared<coco::timer_data_logger>();
			COCO_ASSERT(false, "Logger already exists!");
		}

		void remove(std::string_view logger_name)
		{
			auto it = m_loggers.find(logger_name);
			if (it != m_loggers.end())
				m_loggers.erase(it);
			COCO_ASSERT(false, "Logger not found!");
		}

		std::shared_ptr<coco::timer_data_logger> get(std::string_view logger_name)
		{
			auto it = m_loggers.find(logger_name);
			if (it != m_loggers.end())
				return m_loggers.at(logger_name);
			COCO_ASSERT(false, "Logger not found");
			return nullptr;
		}

		const std::shared_ptr<coco::timer_data_logger> get(std::string_view logger_name) const
		{
			auto it = m_loggers.find(logger_name);
			if (it != m_loggers.end())
				return m_loggers.at(logger_name);
			COCO_ASSERT(false, "Logger not found");
			return nullptr;
		}

		std::shared_ptr<coco::timer_data_logger> operator[](std::string_view logger_name)
		{
			return get(logger_name);
		}

		const std::shared_ptr<coco::timer_data_logger> operator[](std::string_view logger_name) const
		{
			return get(logger_name);
		}

		void log_all() const
		{
			for (auto& logger : m_loggers)
				logger.second->log_statistics();
		}

		void add_to_all(long long time)
		{
			for (auto& logger : m_loggers)
				logger.second->add_measurement(time);
		}
	private:
		std::unordered_map<std::string_view, std::shared_ptr<coco::timer_data_logger>> m_loggers;
	};

	class timer_controller
	{
	public:
		template <_COCO_CONCEPT_TIMER_T _Timer _COCO_ENABLE_IF_TIMER_T(_Timer)>
		static void start_timer(_Timer& timer)
		{
			timer.start();
		}

		template <_COCO_CONCEPT_TIMER_T _Timer _COCO_ENABLE_IF_TIMER_T(_Timer)>
		static void stop_timer(_Timer& timer)
		{
			timer.stop();
		}

		template <_COCO_CONCEPT_TIMER_T _Timer _COCO_ENABLE_IF_TIMER_T(_Timer)>
		static void reset_timer(_Timer& timer)
		{
			timer.reset();
		}

		template <_COCO_CONCEPT_TIMER_T _Timer _COCO_ENABLE_IF_TIMER_T(_Timer)>
		static void pause_timer(_Timer& timer)
		{
			timer.pause();
		}

		template <_COCO_CONCEPT_TIMER_T _Timer _COCO_ENABLE_IF_TIMER_T(_Timer)>
		static void resume_timer(_Timer& timer)
		{
			timer.resume();
		}

		template <_COCO_CONCEPT_TIMER_T _Timer _COCO_ENABLE_IF_TIMER_T(_Timer)>
		static bool is_timer_running(const _Timer& timer)
		{
			return timer.is_running();
		}

		template <_COCO_CONCEPT_TIMER_T _Timer _COCO_ENABLE_IF_TIMER_T(_Timer)>
		static bool is_timer_paused(const _Timer& timer)
		{
			return timer.is_paused();
		}

		template <_COCO_CONCEPT_TIMER_T _Timer _COCO_ENABLE_IF_TIMER_T(_Timer)>
		static long long get_timer_time(const _Timer& timer)
		{
			return timer.get_time();
		}

		template <_COCO_CONCEPT_TIMER_T _Timer _COCO_ENABLE_IF_TIMER_T(_Timer)>
		static void set_timer_print_state(_Timer& timer, bool state)
		{
			timer.set_print_state(state);
		}
	};

	template <class FunT, class ...Args>
	void measure(size_t test_count, const std::filesystem::path& filepath, FunT fun, Args&&... args)
	{
		timer ctimer(dont_start{});
		timer_data_logger measurement_stats;
		for (size_t i = 0; i < test_count; ++i)
		{
			ctimer.start();
			fun(args...);
			ctimer.stop();
			measurement_stats.add_measurement(ctimer.get_time());
		}
		measurement_stats.log_statistics(filepath);
	}
}

#define _COCO_CONCAT_H(x, y)	x##y
#define _COCO_CONCAT(x, y)		_COCO_CONCAT_H(x,y)
#define _COCO_ADD_COUNTER(x)	_COCO_CONCAT(x, __COUNTER__)

//#define COCO_NO_PROFILE

#ifndef COCO_NO_PROFILE
#if defined(__GNUC__) || (defined(__MWERKS__) && (__MWERKS__ >= 0x3000)) || (defined(__ICC) && (__ICC >= 600)) || defined(__ghs__)
#define _COCO_FUNC_SIG __PRETTY_FUNCTION__
#elif defined(__DMC__) && (__DMC__ >= 0x810)
#define _COCO_FUNC_SIG __PRETTY_FUNCTION__
#elif defined(__FUNCSIG__)
#define _COCO_FUNC_SIG __FUNCSIG__
#elif (defined(__INTEL_COMPILER) && (__INTEL_COMPILER >= 600)) || (defined(__IBMCPP__) && (__IBMCPP__ >= 500))
#define _COCO_FUNC_SIG __FUNCTION__
#elif defined(__BORLANDC__) && (__BORLANDC__ >= 0x550)
#define _COCO_FUNC_SIG __FUNC__
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901)
#define _COCO_FUNC_SIG __func__
#elif defined(__cplusplus) && (__cplusplus >= 201103)
#define _COCO_FUNC_SIG __func__
#else
#define _COCO_FUNC_SIG "_COCO_FUNC_SIG unknown!"
#endif

// json
#define COCO_PROFILE_BEGIN_SESSION(name, filepath)	coco::instrumentor::get().begin_session(name, filepath)
#define COCO_PROFILE_END_SESSION()					coco::instrumentor::get().end_session()
#define COCO_PROFILE_SCOPE(name)					coco::instrumentation_timer _COCO_ADD_COUNTER(timer)(name)
#define COCO_PROFILE_FUNCTION()						COCO_PROFILE_SCOPE(_COCO_FUNC_SIG)
#else // COCO_NO_PROFILE
#define COCO_PROFILE_BEGIN_SESSION(name, filepath)
#define COCO_PROFILE_END_SESSION()
#define COCO_PROFILE_SCOPE(name)
#define COCO_PROFILE_FUNCTION()
#endif  // COCO_NO_PROFILE

// console
#define COCO_SCOPE_TIMER()							coco::timer<coco::time_units::microseconds> _COCO_ADD_COUNTER(__coco_timer_var_){ "Coco Timer", true }
#define COCO_SCOPE_TIMER_NAMED(name)				coco::timer<coco::time_units::microseconds> _COCO_ADD_COUNTER(__coco_timer_var_)(name, true)

#define COCO_BEGIN_TIMER_PRINTABLE(timer_name)		coco::timer<coco::time_units::microseconds> _COCO_CONCAT(__coco_time_var_, timer_name){ "Coco Timer", true }
#define COCO_BEGIN_TIMER(timer_name)				coco::timer<coco::time_units::microseconds> _COCO_CONCAT(__coco_time_var_, timer_name)
#define COCO_END_TIMER(timer_name)					((_COCO_CONCAT(__coco_time_var_, timer_name)).stop())
#define COCO_GET_TIMER_VALUE(timer_name)			((_COCO_CONCAT(__coco_time_var_, timer_name)).get_time())

#undef _COCO_ENABLE_IF_DURATION_T
#undef _COCO_CONCEPT_DURATION_T
#undef _COCO_ENABLE_IF_TIMER_T
#undef _COCO_CONCEPT_TIMER_T
