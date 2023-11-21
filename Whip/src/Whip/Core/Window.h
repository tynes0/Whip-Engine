#pragma once

#include "whippch.h"

#include <Whip/Events/Event.h>


_WHIP_START

// window properties
struct window_props
{
	std::string m_title;
	unsigned int m_width;
	unsigned int m_height;

	window_props(const std::string& title = "Whip Engine", unsigned int width = 1280, unsigned int height = 720)
		: m_title(title), m_width(width), m_height(height) {}
};

// interface representing a desktop system  based window

class window
{
public:
	using event_callback_fn = std::function<void(event&)>;
	virtual ~window() {}

	virtual void on_update() = 0;

	WHP_NODISCARD virtual unsigned int get_width() const = 0;
	WHP_NODISCARD virtual unsigned int get_height() const = 0;

	// Window attributes
	virtual void set_event_callback(const event_callback_fn& callback) = 0;
	virtual void set_vsync(bool enabled) = 0;
	WHP_NODISCARD virtual bool is_vsync() const = 0;

	WHP_NODISCARD virtual void* get_native_window() const = 0;

	WHP_NODISCARD static window* create(const window_props& props = window_props());
};

struct window_data
{
	window_props win_props;
	bool vsync;

	window::event_callback_fn event_callback;
};

_WHIP_END