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
    instrumentation_timer(const std::string& name)
        : m_name(name), m_stopped(false)
    {
        m_start_timepoint = std::chrono::high_resolution_clock::now();
    }

    ~instrumentation_timer()
    {
        stop();
    }

    void start()
    {
        if (m_stopped)
        {
            m_start_timepoint = std::chrono::high_resolution_clock::now();
            m_stopped = false;
        }
    }

    void stop()
    {
        if (!m_stopped)
        {
            auto end_timepoint = std::chrono::high_resolution_clock::now();

            long long start = std::chrono::time_point_cast<std::chrono::microseconds>(m_start_timepoint).time_since_epoch().count();
            long long end = std::chrono::time_point_cast<std::chrono::microseconds>(end_timepoint).time_since_epoch().count();

            size_t threadID = std::hash<std::thread::id>{}(std::this_thread::get_id());
            instrumentor::get().write_profile({ m_name, start, end, threadID });
            m_stopped = true;
        }
    }
private:
    std::string m_name;
    std::chrono::time_point<std::chrono::high_resolution_clock> m_start_timepoint;
    bool m_stopped;
};

class console_timer
{
public:
    console_timer(const std::string& name = "console instrumentor")
        : m_name(name), m_stopped(false)
    {
        m_start_timepoint = std::chrono::high_resolution_clock::now();
    }

    ~console_timer()
    {
        stop();
    }

    void start()
    {
        m_start_timepoint = std::chrono::high_resolution_clock::now();
        m_stopped = false;
    }

    void stop()
    {
        if (!m_stopped)
        {
            auto end_timepoint = std::chrono::high_resolution_clock::now();

            long long l_start = std::chrono::time_point_cast<std::chrono::microseconds>(m_start_timepoint).time_since_epoch().count();
            long long l_end = std::chrono::time_point_cast<std::chrono::microseconds>(end_timepoint).time_since_epoch().count();

            WHP_CORE_DEBUG("{0} -> {1} microseconds", m_name, l_end - l_start);

            m_stopped = true;
        }
    }
private:
    std::string m_name;
    std::chrono::time_point<std::chrono::high_resolution_clock> m_start_timepoint;
    bool m_stopped;
};


_WHIP_END


/* THIS IS THE NORMAL CASE but for development i'm closing by hand sometimes
#if defined(_DEBUG)
#define WHP_PROFILE
#endif
*/
#ifdef WHP_PROFILE
    #if defined(__GNUC__) || (defined(__MWERKS__) && (__MWERKS__ >= 0x3000)) || (defined(__ICC) && (__ICC >= 600)) || defined(__ghs__)
        #define WHP_FUNC_SIG __PRETTY_FUNCTION__
    #elif defined(__DMC__) && (__DMC__ >= 0x810)
        #define WHP_FUNC_SIG __PRETTY_FUNCTION__
    #elif defined(__FUNCSIG__)
        #define WHP_FUNC_SIG __FUNCSIG__
    #elif (defined(__INTEL_COMPILER) && (__INTEL_COMPILER >= 600)) || (defined(__IBMCPP__) && (__IBMCPP__ >= 500))
        #define WHP_FUNC_SIG __FUNCTION__
    #elif defined(__BORLANDC__) && (__BORLANDC__ >= 0x550)
        #define WHP_FUNC_SIG __FUNC__
    #elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901)
        #define WHP_FUNC_SIG __func__
    #elif defined(__cplusplus) && (__cplusplus >= 201103)
        #define WHP_FUNC_SIG __func__
    #else
        #define WHP_FUNC_SIG "WHP_FUNC_SIG unknown!"
    #endif
    
    #define WHP_PROFILE_BEGIN_SESSION(name, filepath) _WHIP instrumentor::get().begin_session(name, filepath)
    #define WHP_PROFILE_END_SESSION() _WHIP instrumentor::get().end_session()
    #define WHP_PROFILE_SCOPE(name) _WHIP instrumentation_timer timer##__COUNTER__(name)
    #define WHP_PROFILE_FUNCTION() WHP_PROFILE_SCOPE(WHP_FUNC_SIG)
#else // WHP_PROFILE
    #define WHP_PROFILE_BEGIN_SESSION(name, filepath)
    #define WHP_PROFILE_END_SESSION()
    #define WHP_PROFILE_SCOPE(name)
    #define WHP_PROFILE_FUNCTION()
#endif  // WHP_PROFILE