#pragma once

#include <whippch.h>

#include <Whip/Events/Event.h>


_WHIP_START

// window properties
struct window_props
{
	std::string Title;
	unsigned int Width;
	unsigned int Height;

	window_props(const std::string& title = "Whip Engine", unsigned int width = 1280, unsigned int height = 720)
		: Title(title), Width(width), Height(height) {}

};

// interface representing a desktop system  based window

class WHIP_API Window
{
public:
	using event_callback_fn = std::function<void(Event&)>;
	virtual ~Window() {}

	virtual void OnUpdate() = 0;

	WHP_NODISCARD virtual unsigned int get_width() const = 0;
	WHP_NODISCARD virtual unsigned int get_height() const = 0;

	// Window attributes
	virtual void set_event_callback(const event_callback_fn& callback) = 0;
	virtual void set_vsync(bool enabled) = 0;
	WHP_NODISCARD virtual bool is_vsync() const = 0;

	WHP_NODISCARD virtual void* get_native_window() const = 0;

	WHP_NODISCARD static Window* create(const window_props& props = window_props());
};

struct window_data
{
	window_props win_props;
	bool VSync;

	Window::event_callback_fn EventCallback;
};

_WHIP_END