#pragma once

#include <Whip/Core.h>

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

	float get_seconds() const { return m_time; }
	float get_milliseconds() const { return (m_time * 1000.0f); }
};

_WHIP_END