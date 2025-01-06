#pragma once

#include <Whip/Events/Event.h>

#include <Whip/Core/MouseButtonCodes.h>

#include <sstream>

_WHIP_START

class mouse_moved_event : public event
{
private:
	float m_mouse_x, m_mouse_y;
public:
	mouse_moved_event(const float x, const float y) : m_mouse_x(x), m_mouse_y(y) {}

	WHP_NODISCARD inline float get_x() const { return m_mouse_x; }
	WHP_NODISCARD inline float get_y() const { return m_mouse_y; }

	EVENT_TO_STRING
	{
		std::stringstream ss;
		ss << "mouse_moved_event: " << m_mouse_x << ", " << m_mouse_y;
		return ss.str();
	}
	EVENT_CLASS_CATEGORY(event_category_mouse | event_category_input)
	EVENT_CLASS_TYPE(mouse_moved)
};

class mouse_scrolled_event : public event
{
private:
	float m_offset_x, m_offset_y;
public:
	mouse_scrolled_event(const float offset_x, const float offset_y) : m_offset_x(offset_x), m_offset_y(offset_y) {}

	WHP_NODISCARD float get_offset_x() const { return m_offset_x; }
	WHP_NODISCARD float get_offset_y() const { return m_offset_y; }

	EVENT_TO_STRING
	{
		std::stringstream ss;
		ss << "mouse_scrolled_event: " << m_offset_x << ", " << m_offset_y;
		return ss.str();
	}

	EVENT_CLASS_CATEGORY(event_category_mouse | event_category_input)
	EVENT_CLASS_TYPE(mouse_scrolled)
};

class mouse_button_event : public event
{
protected:
	int m_button;

	mouse_button_event(const mouse_code button) : m_button(button) {}
public:
	WHP_NODISCARD int get_mouse_button() const { return m_button; }
	EVENT_CLASS_CATEGORY(event_category_mouse | event_category_input | event_category_mouse_button)
};

class mouse_button_pressed_event : public mouse_button_event
{
public:
	mouse_button_pressed_event(const mouse_code button) : mouse_button_event(button) {}

	EVENT_TO_STRING
	{
		std::stringstream ss;
		ss << "mouse_button_pressed_event: " << m_button;
		return ss.str();
	}

	EVENT_CLASS_TYPE(mouse_button_pressed)
};

class mouse_button_released_event : public mouse_button_event
{
public:
	mouse_button_released_event(const mouse_code button) : mouse_button_event(button) {}

	EVENT_TO_STRING
	{
		std::stringstream ss;
		ss << "mouse_button_released_event: " << m_button;
		return ss.str();
	}
	
	EVENT_CLASS_TYPE(mouse_button_released)
};

_WHIP_END
