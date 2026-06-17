#pragma once

#include <Whip/Core/Core.h>

_WHIP_START

// Helper class

// time default = second
class Timestep
{
public:
	Timestep(float time = 0.0f) : m_Time(time) {}

	operator float() const { return m_Time; }

	WHP_NODISCARD float GetSeconds() const { return m_Time; }
	WHP_NODISCARD float GetMinutes() const { return (GetSeconds() / 60.0f); }
	WHP_NODISCARD float GetHours() const { return (GetMinutes() / 60.0f); }
	WHP_NODISCARD float GetSplitSeconds() const { return (GetSeconds() * 60.0f); }
	WHP_NODISCARD float GetMilliseconds() const { return (GetSeconds() * 1000.0f); }
	WHP_NODISCARD float GetMicroseconds() const { return (GetMilliseconds() * 1000.0f); }
	WHP_NODISCARD float GetNanoseconds() const { return (GetMicroseconds() * 1000.0f); }
private:
	float m_Time;
};

_WHIP_END
