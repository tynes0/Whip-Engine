#pragma once

#include <Whip/Window.h>
#include <Whip/Render/GraphicsContext.h>

#include <GLFW/glfw3.h>


_WHIP_START

class WindowsWindow : public Window
{
private:
	GLFWwindow* m_window;
	GraphicsContext* m_context;
	window_data m_data;
private:
	virtual void init(const window_props& props);
	virtual void shutdown();
public:
	WindowsWindow(const window_props& props);
	virtual ~WindowsWindow();

	void OnUpdate() override;

	WHP_NODISCARD inline unsigned int get_width() const override { return m_data.win_props.Width; }
	WHP_NODISCARD inline unsigned int get_height() const override { return m_data.win_props.Height; }

	WHP_NODISCARD inline virtual void* get_native_window() const override { return m_window; }

	// Window attributes
	inline void set_event_callback(const event_callback_fn& callback) override { m_data.EventCallback = callback; }
	void set_vsync(bool enabled) override;
	WHP_NODISCARD bool is_vsync() const override;
};

#define RESIZE_WIN_PROPS(win_props, width, height) win_props.Width = width, win_props.Height = height
#define SET_WIN_PROPS(win_props, title, width, height) RESIZE_WIN_PROPS(win_props, width, height), win_props.Title = title

_WHIP_END