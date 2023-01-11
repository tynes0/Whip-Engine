#pragma once

#include <Whip/Events/Event.h>

_WHIP_START

class WHIP_API KeyEvent : public Event
{
protected:
	KeyEvent(int keycode) : m_KeyCode(keycode) {}

	int m_KeyCode;
public:
	inline int GetKeycode() const { return m_KeyCode; }
	EVENT_CLASS_CATEGORY(EventCategoryKeyboard | EventCategoryInput)
};

class WHIP_API KeyPressedEvent : public KeyEvent
{
private:
	int m_RepeatCount;
public:
	KeyPressedEvent(int keycode, int repeatCount)
		: KeyEvent(keycode), m_RepeatCount(repeatCount) {}

	inline int GetRepeatCount() const { return m_RepeatCount; }

	std::string ToString() const override
	{
		std::stringstream ss;
		ss << "KeyPressedEvent: " << m_KeyCode << " (" << m_RepeatCount << " repeats)";
		return ss.str();
	}

	EVENT_CLASS_TYPE(KeyPressed)
};

class WHIP_API KeyReleasedEvent : public KeyEvent
{
public:
	KeyReleasedEvent(int keycode) : KeyEvent(keycode) {}

	std::string ToString() const override
	{
		std::stringstream ss;
		ss << "KeyReleasedEvent: " << m_KeyCode;
		return ss.str();
	}

	EVENT_CLASS_TYPE(KeyReleased)
};


_WHIP_END