#pragma once

#include <Whip/Core/Core.h>

_WHIP_START

// Helper class

// time default = second
class timestep
{
private:
	float m_time;
public:
	timestep(float time = 0.0f) : m_time(time) {}

	operator float() const { return m_time; }

	WHP_NODISCARD float get_seconds() const { return m_time; }
	WHP_NODISCARD float get_minutes() const { return (get_seconds() / 60.0f); }
	WHP_NODISCARD float get_hours() const { return (get_minutes() / 60.0f); }
	WHP_NODISCARD float get_split_seconds() const { return (get_seconds() * 60.0f); }
	WHP_NODISCARD float get_milliseconds() const { return (get_seconds() * 1000.0f); }
	WHP_NODISCARD float get_microseconds() const { return (get_milliseconds() * 1000.0f); }
	WHP_NODISCARD float get_nanoseconds() const { return (get_microseconds() * 1000.0f); }
};

_WHIP_END