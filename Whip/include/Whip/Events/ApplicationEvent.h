#pragma once

#include <Whip/Events/Event.h>

#include <vector>
#include <filesystem>

_WHIP_START

class WindowResizeEvent : public Event
{
private:
	uint32_t m_Width, m_Height;
public:
	WindowResizeEvent(uint32_t width, uint32_t height) : m_Width(width), m_Height(height) {}

	WHP_NODISCARD inline uint32_t GetWidth() const { return m_Width; }
	WHP_NODISCARD inline uint32_t GetHeight() const { return m_Height; }

	EVENT_TO_STRING
	{
		std::stringstream ss;
		ss << "WindowResizeEvent: " << m_Width << ", " << m_Height;
		return ss.str();
	}

	EVENT_CLASS_CATEGORY(EventCategoryApplication)
	EVENT_CLASS_TYPE(WindowResize)
};

class WindowCloseEvent : public Event
{
public:
	WindowCloseEvent() {}

	EVENT_TO_STRING
	{
		return "Window closed";
	}

	EVENT_CLASS_CATEGORY(EventCategoryApplication)
	EVENT_CLASS_TYPE(WindowClose)
};

class WindowDropEvent : public Event
{
public:
	WindowDropEvent(const std::vector<std::filesystem::path>& paths) : m_Paths(paths) {}

	WindowDropEvent(std::vector<std::filesystem::path>&& paths) : m_Paths(std::move(paths)) {}

	const std::vector<std::filesystem::path>& GetPaths() const { return m_Paths; }

	EVENT_CLASS_CATEGORY(EventCategoryApplication)
	EVENT_CLASS_TYPE(WindowDrop)
private:
	std::vector<std::filesystem::path> m_Paths;
};

class AppTickEvent : public Event
{
public:
	AppTickEvent() {}

	EVENT_CLASS_CATEGORY(EventCategoryApplication)
	EVENT_CLASS_TYPE(AppTick)
};

class AppUpdateEvent : public Event
{
public:
	AppUpdateEvent() {}

	EVENT_CLASS_CATEGORY(EventCategoryApplication)
	EVENT_CLASS_TYPE(AppUpdate)
};

class AppRenderEvent : public Event
{
public:
	AppRenderEvent() {}

	EVENT_CLASS_CATEGORY(EventCategoryApplication)
	EVENT_CLASS_TYPE(AppRender)
};

_WHIP_END
