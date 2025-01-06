#pragma once

#include <Whip/Events/Event.h>
#include <Whip/Core/KeyCodes.h>

_WHIP_START

// repeat_time type
using repeat_t = uint16_t;

class key_event : public event
{
public:
	WHP_NODISCARD inline key_code get_keycode() const { return m_key_code; }
	EVENT_CLASS_CATEGORY(event_category_keyboard | event_category_input)
protected:
	key_event(const key_code keycode) : m_key_code(keycode) {}

	int m_key_code;
};

class key_pressed_event : public key_event
{
public:
	key_pressed_event(const key_code keycode, repeat_t repeatCount) : key_event(keycode), m_repeat_count(repeatCount) {}

	WHP_NODISCARD inline repeat_t get_repeat_count() const { return m_repeat_count; }

	EVENT_TO_STRING
	{
		std::stringstream ss;
		ss << "key_pressed_event: " << m_key_code << " (" << m_repeat_count << " repeats)";
		return ss.str();
	}

	EVENT_CLASS_TYPE(key_pressed)
private:
	repeat_t m_repeat_count;
};

class key_released_event : public key_event
{
public:
	key_released_event(const key_code keycode) : key_event(keycode) {}

	EVENT_TO_STRING
	{
		std::stringstream ss;
		ss << "key_released_event: " << m_key_code;
		return ss.str();
	}

	EVENT_CLASS_TYPE(key_released)
};

class key_typed_event : public key_event
{
public:
	key_typed_event(const key_code keycode) : key_event(keycode) {}

	EVENT_TO_STRING
	{
		std::stringstream ss;
		ss << "key_typed_event: " << m_key_code;
		return ss.str();
	}

		EVENT_CLASS_TYPE(key_typed)
};

_WHIP_END