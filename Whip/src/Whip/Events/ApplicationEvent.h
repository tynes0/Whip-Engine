#pragma once

#include <Whip/Events/Event.h>

#include <vector>
#include <filesystem>

_WHIP_START

class window_resize_event : public event
{
private:
	uint32_t m_width, m_height;
public:
	window_resize_event(uint32_t width, uint32_t height) : m_width(width), m_height(height) {}

	WHP_NODISCARD inline uint32_t get_width() const { return m_width; }
	WHP_NODISCARD inline uint32_t get_height() const { return m_height; }

	EVENT_TO_STRING
	{
		std::stringstream ss;
		ss << "window_resize_event: " << m_width << ", " << m_height;
		return ss.str();
	}

	EVENT_CLASS_CATEGORY(event_category_application)
	EVENT_CLASS_TYPE(window_resize)
};

class window_close_event : public event
{
public:
	window_close_event() {}

	EVENT_TO_STRING
	{
		return "Window closed";
	}

	EVENT_CLASS_CATEGORY(event_category_application)
	EVENT_CLASS_TYPE(window_close)
};

class window_drop_event : public event
{
public:
	window_drop_event(const std::vector<std::filesystem::path>& paths) : m_paths(paths) {}

	window_drop_event(std::vector<std::filesystem::path>&& paths) : m_paths(std::move(paths)) {}

	const std::vector<std::filesystem::path>& get_paths() const { return m_paths; }

	EVENT_CLASS_CATEGORY(event_category_application)
	EVENT_CLASS_TYPE(window_drop)
private:
	std::vector<std::filesystem::path> m_paths;
};

class app_tick_event : public event
{
public:
	app_tick_event() {}

	EVENT_CLASS_CATEGORY(event_category_application)
	EVENT_CLASS_TYPE(app_tick)
};

class app_update_event : public event
{
public:
	app_update_event() {}

	EVENT_CLASS_CATEGORY(event_category_application)
	EVENT_CLASS_TYPE(app_update)
};

class app_render_event : public event
{
public:
	app_render_event() {}

	EVENT_CLASS_CATEGORY(event_category_application)
	EVENT_CLASS_TYPE(app_render)
};

_WHIP_END
