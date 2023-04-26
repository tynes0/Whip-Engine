#pragma once

#include <Whip/Core/Core.h>


#include <string>
#include <chrono>
#include <algorithm>
#include <fstream>

#include <thread>

_WHIP_START

struct profile_result
{
    std::string name;
    long long start, end;
    size_t threadID;
};

struct instrumentation_session
{
    std::string Name;
};

class instrumentor
{
private:
    instrumentation_session* m_current_session;
    std::ofstream m_output_stream;
    int m_profile_count;
public:
    instrumentor()
        : m_current_session(nullptr), m_profile_count(0)
    {
    }

    void begin_session(const std::string& name, const std::string& filepath = "results.json")
    {
        m_output_stream.open(filepath);
        write_header();
        m_current_session = new instrumentation_session{ name };
    }

    void end_session()
    {
        write_footer();
        m_output_stream.close();
        delete m_current_session;
        m_current_session = nullptr;
        m_profile_count = 0;
    }

    void write_profile(const profile_result& result)
    {
        if (m_profile_count++ > 0)
            m_output_stream << ",";

        std::string name = result.name;
        std::replace(name.begin(), name.end(), '"', '\'');

        m_output_stream << "{";
        m_output_stream << "\"cat\":\"function\",";
        m_output_stream << "\"dur\":" << (result.end - result.start) << ',';
        m_output_stream << "\"name\":\"" << name << "\",";
        m_output_stream << "\"ph\":\"X\",";
        m_output_stream << "\"pid\":0,";
        m_output_stream << "\"tid\":" << result.threadID << ",";
        m_output_stream << "\"ts\":" << result.start;
        m_output_stream << "}";

        m_output_stream.flush();
    }

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

    static instrumentor& get()
    {
        static instrumentor instance;
        return instance;
    }
};

class instrumentation_timer
{
public:
    instrumentation_timer(const char* name)
        : m_name(name), m_stopped(false)
    {
        m_start_timepoint = std::chrono::high_resolution_clock::now();
    }

    ~instrumentation_timer()
    {
        if (!m_stopped)
            stop();
    }

    void stop()
    {
        auto end_timepoint = std::chrono::high_resolution_clock::now();

        long long start = std::chrono::time_point_cast<std::chrono::microseconds>(m_start_timepoint).time_since_epoch().count();
        long long end = std::chrono::time_point_cast<std::chrono::microseconds>(end_timepoint).time_since_epoch().count();

        size_t threadID = std::hash<std::thread::id>{}(std::this_thread::get_id());
        instrumentor::get().write_profile({ m_name, start, end, threadID });
        m_stopped = true;
    }
private:
    const char* m_name;
    std::chrono::time_point<std::chrono::high_resolution_clock> m_start_timepoint;
    bool m_stopped;
};

_WHIP_END

#if !WHP_PROFILE
#define WHP_PROFILE_BEGIN_SESSION(name, filepath) _WHIP instrumentor::get().begin_session(name, filepath)
#define WHP_PROFILE_END_SESSION() _WHIP instrumentor::get().end_session()
#define WHP_PROFILE_SCOPE(name) _WHIP instrumentation_timer timer##__COUNTER__(name)
#define WHP_PROFILE_FUNCTION() WHP_PROFILE_SCOPE(__FUNCSIG__)
#else // WHP_PROFILE
#define WHP_PROFILE_BEGIN_SESSION(name, filepath)
#define WHP_PROFILE_END_SESSION()
#define WHP_PROFILE_SCOPE(name)
#define WHP_PROFILE_FUNCTION()
#endif  // WHP_PROFILE