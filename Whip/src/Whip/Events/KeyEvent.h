#pragma once

#include <Whip/Events/Event.h>

_WHIP_START

// repeat_time type
using repeat_t = unsigned long long;

class key_event : public event
{
protected:
	key_event(int keycode) : m_key_code(keycode) {}

	int m_key_code;
public:
	WHP_NODISCARD inline int get_keycode() const { return m_key_code; }
	EVENT_CLASS_CATEGORY(event_category_keyboard | event_category_input)
};

class key_pressed_event : public key_event
{
private:
	repeat_t m_repeat_count;
public:
	key_pressed_event(int keycode, repeat_t repeatCount)
		: key_event(keycode), m_repeat_count(repeatCount) {}

	WHP_NODISCARD inline repeat_t get_repeat_count() const { return m_repeat_count; }

	EVENT_TO_STRING
	{
		std::stringstream ss;
		ss << "key_pressed_event: " << m_key_code << " (" << m_repeat_count << " repeats)";
		return ss.str();
	}

	EVENT_CLASS_TYPE(key_pressed)
};

class key_released_event : public key_event
{
public:
	key_released_event(int keycode) : key_event(keycode) {}

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
	key_typed_event(int keycode) : key_event(keycode) {}

	EVENT_TO_STRING
	{
		std::stringstream ss;
		ss << "key_typed_event: " << m_key_code;
		return ss.str();
	}

		EVENT_CLASS_TYPE(key_typed)
};

_WHIP_END