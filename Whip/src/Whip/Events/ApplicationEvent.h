#pragma once

#include <Whip/Events/Event.h>

_WHIP_START

class WHIP_API WindowResizeEvent : public Event
{
private:
	unsigned int m_Width, m_Height;
public:
	WindowResizeEvent(unsigned int width, unsigned int height) : m_Width(width), m_Height(height) {}

	WHP_NODISCARD inline unsigned int GetWidth() const { return m_Width; }
	WHP_NODISCARD inline unsigned int GetHeight() const { return m_Height; }

	EVENT_TO_STRING
	{
		std::stringstream ss;
		ss << "WindowResizeEvent: " << m_Width << ", " << m_Height;
		return ss.str();
	}

	EVENT_CLASS_CATEGORY(EventCategoryApplication)
	EVENT_CLASS_TYPE(WindowResize)
};

class WHIP_API WindowCloseEvent : public Event
{
public:
	WindowCloseEvent() {}

	EVENT_TO_STRING
	{
		return "WindowClosed";
	}

	EVENT_CLASS_CATEGORY(EventCategoryApplication)
	EVENT_CLASS_TYPE(WindowClose)
};

class WHIP_API AppTickEvent : public Event
{
public:
	AppTickEvent() {}

	EVENT_CLASS_CATEGORY(EventCategoryApplication)
	EVENT_CLASS_TYPE(AppTick)
};

class WHIP_API AppUpdateEvent : public Event
{
public:
	AppUpdateEvent() {}

	EVENT_CLASS_CATEGORY(EventCategoryApplication)
	EVENT_CLASS_TYPE(AppUpdate)
};

class WHIP_API AppRenderEvent : public Event
{
public:
	AppRenderEvent() {}

	EVENT_CLASS_CATEGORY(EventCategoryApplication)
	EVENT_CLASS_TYPE(AppRender)
};

_WHIP_END