#pragma once

#include <Whip/Events/Event.h>
#include <Whip/Core/KeyCodes.h>

_WHIP_START

// Repeat time type.
using RepeatType = uint16_t;

class KeyEvent : public Event
{
public:
	WHP_NODISCARD inline KeyCode GetKeyCode() const { return m_KeyCode; }
	EVENT_CLASS_CATEGORY(EventCategoryKeyboard | EventCategoryInput)
protected:
	KeyEvent(const KeyCode keyCode) : m_KeyCode(keyCode) {}

	int m_KeyCode;
};

class KeyPressedEvent : public KeyEvent
{
public:
	KeyPressedEvent(const KeyCode keyCode, RepeatType repeatCount) : KeyEvent(keyCode), m_RepeatCount(repeatCount) {}

	WHP_NODISCARD inline RepeatType GetRepeatCount() const { return m_RepeatCount; }

	EVENT_TO_STRING
	{
		std::stringstream ss;
		ss << "KeyPressedEvent: " << m_KeyCode << " (" << m_RepeatCount << " repeats)";
		return ss.str();
	}

	EVENT_CLASS_TYPE(KeyPressed)
private:
	RepeatType m_RepeatCount;
};

class KeyReleasedEvent : public KeyEvent
{
public:
	KeyReleasedEvent(const KeyCode keyCode) : KeyEvent(keyCode) {}

	EVENT_TO_STRING
	{
		std::stringstream ss;
		ss << "KeyReleasedEvent: " << m_KeyCode;
		return ss.str();
	}

	EVENT_CLASS_TYPE(KeyReleased)
};

class KeyTypedEvent : public KeyEvent
{
public:
	KeyTypedEvent(const KeyCode keyCode) : KeyEvent(keyCode) {}

	EVENT_TO_STRING
	{
		std::stringstream ss;
		ss << "KeyTypedEvent: " << m_KeyCode;
		return ss.str();
	}

		EVENT_CLASS_TYPE(KeyTyped)
};

_WHIP_END
