#pragma once

#include <Whip/Events/Event.h>

#include <Whip/Core/MouseButtonCodes.h>

#include <sstream>

_WHIP_START

class MouseMovedEvent : public Event
{
private:
	float m_MouseX, m_MouseY;
public:
	MouseMovedEvent(const float x, const float y) : m_MouseX(x), m_MouseY(y) {}

	WHP_NODISCARD inline float GetX() const { return m_MouseX; }
	WHP_NODISCARD inline float GetY() const { return m_MouseY; }

	EVENT_TO_STRING
	{
		std::stringstream ss;
		ss << "MouseMovedEvent: " << m_MouseX << ", " << m_MouseY;
		return ss.str();
	}
	EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput)
	EVENT_CLASS_TYPE(MouseMoved)
};

class MouseScrolledEvent : public Event
{
private:
	float m_OffsetX, m_OffsetY;
public:
	MouseScrolledEvent(const float offsetX, const float offsetY) : m_OffsetX(offsetX), m_OffsetY(offsetY) {}

	WHP_NODISCARD float GetOffsetX() const { return m_OffsetX; }
	WHP_NODISCARD float GetOffsetY() const { return m_OffsetY; }

	EVENT_TO_STRING
	{
		std::stringstream ss;
		ss << "MouseScrolledEvent: " << m_OffsetX << ", " << m_OffsetY;
		return ss.str();
	}

	EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput)
	EVENT_CLASS_TYPE(MouseScrolled)
};

class MouseButtonEvent : public Event
{
protected:
	int m_Button;

	MouseButtonEvent(const MouseCode button) : m_Button(button) {}
public:
	WHP_NODISCARD int GetMouseButton() const { return m_Button; }
	EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput | EventCategoryMouseButton)
};

class MouseButtonPressedEvent : public MouseButtonEvent
{
public:
	MouseButtonPressedEvent(const MouseCode button) : MouseButtonEvent(button) {}

	EVENT_TO_STRING
	{
		std::stringstream ss;
		ss << "MouseButtonPressedEvent: " << m_Button;
		return ss.str();
	}

	EVENT_CLASS_TYPE(MouseButtonPressed)
};

class MouseButtonReleasedEvent : public MouseButtonEvent
{
public:
	MouseButtonReleasedEvent(const MouseCode button) : MouseButtonEvent(button) {}

	EVENT_TO_STRING
	{
		std::stringstream ss;
		ss << "MouseButtonReleasedEvent: " << m_Button;
		return ss.str();
	}
	
	EVENT_CLASS_TYPE(MouseButtonReleased)
};

_WHIP_END
