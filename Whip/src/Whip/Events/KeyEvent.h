#pragma once

#include <Whip/Events/Event.h>

_WHIP_START

class WHIP_API KeyEvent : public Event
{
protected:
	KeyEvent(int keycode) : m_KeyCode(keycode) {}

	int m_KeyCode;
public:
	WHP_NODISCARD inline int GetKeycode() const { return m_KeyCode; }
	EVENT_CLASS_CATEGORY(EventCategoryKeyboard | EventCategoryInput)
};

class WHIP_API KeyPressedEvent : public KeyEvent
{
private:
	int m_RepeatCount;
public:
	KeyPressedEvent(int keycode, int repeatCount)
		: KeyEvent(keycode), m_RepeatCount(repeatCount) {}

	WHP_NODISCARD inline int GetRepeatCount() const { return m_RepeatCount; }

	EVENT_TO_STRING
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

	EVENT_TO_STRING
	{
		std::stringstream ss;
		ss << "KeyReleasedEvent: " << m_KeyCode;
		return ss.str();
	}

	EVENT_CLASS_TYPE(KeyReleased)
};

class WHIP_API KeyTypedEvent : public KeyEvent
{
public:
	KeyTypedEvent(int keycode) : KeyEvent(keycode) {}

	EVENT_TO_STRING
	{
		std::stringstream ss;
		ss << "KeyTypedEvent: " << m_KeyCode;
		return ss.str();
	}

		EVENT_CLASS_TYPE(KeyTyped)
};

_WHIP_END