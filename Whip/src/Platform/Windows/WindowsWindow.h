#pragma once

#include <Whip/Core/Window.h>
#include <Whip/Render/GraphicsContext.h>

#include <GLFW/glfw3.h>


_WHIP_START

class windows_window : public window
{
private:
	GLFWwindow* m_window;
	graphic_context* m_context;
	window_data m_data;
private:
	virtual void init(const window_props& props);
	virtual void shutdown();
public:
	windows_window(const window_props& props);
	virtual ~windows_window();

	void on_update() override;

	WHP_NODISCARD inline unsigned int get_width() const override { return m_data.win_props.m_width; }
	WHP_NODISCARD inline unsigned int get_height() const override { return m_data.win_props.m_height; }

	WHP_NODISCARD inline virtual void* get_native_window() const override { return m_window; }

	// Window attributes
	inline void set_event_callback(const event_callback_fn& callback) override { m_data.event_callback = callback; }
	void set_vsync(bool enabled) override;
	WHP_NODISCARD bool is_vsync() const override;
};

#define RESIZE_WIN_PROPS(win_props, width, height) win_props.m_width = width, win_props.m_height = height
#define SET_WIN_PROPS(win_props, title, width, height) RESIZE_WIN_PROPS(win_props, width, height), win_props.m_title = title

_WHIP_END