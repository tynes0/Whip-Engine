#pragma once

#include <Whip/Core/Window.h>
#include <Whip/Render/GraphicsContext.h>

#include <GLFW/glfw3.h>


_WHIP_START

class windows_window : public window
{
public:
	windows_window(const window_props& props);
	virtual ~windows_window();

	void on_update() override;

	WHP_NODISCARD inline unsigned int get_width() const override { return m_data.win_props.width; }
	WHP_NODISCARD inline unsigned int get_height() const override { return m_data.win_props.height; }
	WHP_NODISCARD virtual float get_scroll_delta() const override;

	WHP_NODISCARD std::pair<int, int> get_position() const override;

	WHP_NODISCARD inline virtual void* get_native_window() const override { return m_window; }

	// Window attributes
	inline void set_event_callback(const event_callback_fn& callback) override { m_data.event_callback = callback; }
	void set_vsync(bool enabled) override;
	WHP_NODISCARD bool is_vsync() const override;
private:
	virtual void init(const window_props& props);
	virtual void shutdown();
private:
	GLFWwindow* m_window;
	graphic_context* m_context;
	window_data m_data;
};

_WHIP_END
