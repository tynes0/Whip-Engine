#pragma once

#include <Whip/Core/Window.h>
#include <Whip/Events/ApplicationEvent.h>
#include <Whip/Core/LayerStack.h>
#include <Whip/ImGui/ImGuiLayer.h>
#include <Whip/Core/Timestep.h>

_WHIP_START

class application
{
private:
	static application* s_instance;
	scope<window> m_window;
	imgui_layer* m_imgui_layer;
	layer_stack m_layer_stack;
	bool m_running = true;
	bool m_minimized = false;
	float m_last_frame_time = 0.0f;
private:
	bool on_window_close(window_close_event& evnt);
	bool on_window_resize(window_resize_event& evnt);
public:
	application();
	virtual ~application();

	void run();
	void close();
	void on_event(event& evnt);
	void push_layer(layerptr layer);
	void push_overlay(layerptr overlay);

	WHP_NODISCARD inline static application& get() { return DREF(s_instance); }
	WHP_NODISCARD inline window& get_window() { return DREF(m_window); }
};

// to be defined in client
application* create_application();

_WHIP_END