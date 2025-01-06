#pragma once

#include <sstream>

#include <Whip/Events/Event.h>


_WHIP_START

// window properties
struct window_props
{
	std::string title = "Whip Application";
	uint32_t width = 1280;
	uint32_t height = 720;
	bool fullscreen = false;
};

// interface representing a desktop system  based window
class window
{
public:
	using event_callback_fn = std::function<void(event&)>;
	virtual ~window() {}

	virtual void on_update() = 0;

	WHP_NODISCARD virtual uint32_t get_width() const = 0;
	WHP_NODISCARD virtual uint32_t get_height() const = 0;
	WHP_NODISCARD virtual float get_scroll_delta() const = 0;

	WHP_NODISCARD virtual std::pair<int, int> get_position() const = 0;

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
	bool vsync = false;

	window::event_callback_fn event_callback;
};

_WHIP_END
