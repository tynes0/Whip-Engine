#pragma once

#include <Whip/Events/Event.h>

_WHIP_START

class WHIP_API MouseMovedEvent : public Event
{
private:
	float m_MouseX, m_MouseY;
public:
	MouseMovedEvent(float x, float y) : m_MouseX(x), m_MouseY(y) {}

	inline float GetX() const { return m_MouseX; }
	inline float GetY() const { return m_MouseY; }

	EVENT_TO_STRING
	{
		std::stringstream ss;
		ss << "MouseMovedEvent: " << m_MouseX << ", " << m_MouseY;
		return ss.str();
	}
	EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput)
	EVENT_CLASS_TYPE(MouseMoved)
};

class WHIP_API MouseScrolledEvent : public Event
{
private:
	float m_OffsetX, m_OffsetY;
public:
	MouseScrolledEvent(float OffsetX, float OffsetY) : m_OffsetX(OffsetX), m_OffsetY(OffsetY) {}

	inline float GetOffsetX() const { return m_OffsetX; }
	inline float GetOffsetY() const { return m_OffsetY; }

	EVENT_TO_STRING
	{
		std::stringstream ss;
		ss << "MouseScrolledEvent: " << m_OffsetX << ", " << m_OffsetY;
		return ss.str();
	}

	EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput)
	EVENT_CLASS_TYPE(MouseScrolled)
};

class WHIP_API MouseButtonEvent : public Event
{
protected:
	int m_Button;

	MouseButtonEvent(int button) : m_Button(button) {}
public:
	inline int GetMouseButton() const { return m_Button; }
	EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput | EventCategoryMouseButton)
};

class WHIP_API MouseButtonPressedEvent : public MouseButtonEvent
{
public:
	MouseButtonPressedEvent(int button) : MouseButtonEvent(button) {}

	EVENT_TO_STRING
	{
		std::stringstream ss;
		ss << "MouseButtonPressedEvent: " << m_Button;
		return ss.str();
	}

	EVENT_CLASS_TYPE(MouseButtonPressed)
};

class WHIP_API MouseButtonReleasedEvent : public MouseButtonEvent
{
public:
	MouseButtonReleasedEvent(int button) : MouseButtonEvent(button) {}

	EVENT_TO_STRING
	{
		std::stringstream ss;
		ss << "MouseButtonReleasedEvent: " << m_Button;
		return ss.str();
	}
	
	EVENT_CLASS_TYPE(MouseButtonReleased)
};

_WHIP_END
