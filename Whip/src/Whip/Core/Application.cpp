#include <whippch.h>
#include "Application.h"

#include <Whip/Render/Renderer.h>

#include <GLFW/glfw3.h>

_WHIP_START

application* application::s_instance = nullptr;

application::application()
{
	WHP_CORE_ASSERT(!s_instance, "Application already exist!");
	s_instance = this;
	m_window = scope<window>(window::create());
	m_window->set_event_callback(WHP_BIND_EVENT_FN(application::on_event));
	m_window->set_event_callback(std::bind(&application::on_event, this, std::placeholders::_1));

	renderer::init();

	m_imgui_layer = new imgui_layer();
	push_overlay(m_imgui_layer);
}

application::~application()
{

}

void application::run()
{
	while (m_running)
	{
		float time = (float)glfwGetTime();
		timestep ts = time - m_last_frame_time;
		m_last_frame_time = time;
		//WHP_CORE_DEBUG("Delta time = {0}s ({1}ms)", ts, ts.get_milliseconds());

		if (!m_minimized)
		{
			for (layerptr item : m_layer_stack)
			{
				item->on_update(ts);
			}
		}

		m_imgui_layer->begin();
		for (layerptr item : m_layer_stack)
		{
			item->on_imgui_render();
		}
		m_imgui_layer->end();
		m_window->on_update();
	}
}

void application::on_event(Event& e)
{
	event_dispatcher dispatcher(e);
	dispatcher.dispatch<window_close_event>(WHP_BIND_EVENT_FN(application::on_window_close));
	dispatcher.dispatch<window_resize_event>(WHP_BIND_EVENT_FN(application::on_window_resize));

	for (auto iter = m_layer_stack.end(); iter != m_layer_stack.begin(); )
	{
		(DREF(--iter))->on_event(e);
		if (e.handled)
		{
			break;
		}
	}
}

void application::push_layer(layerptr layer)
{
	m_layer_stack.push_layer(layer);
	layer->on_attach();
}

void application::push_overlay(layerptr overlay)
{
	m_layer_stack.push_overlay(overlay);
	overlay->on_attach();
}

bool application::on_window_close(window_close_event& evnt)
{
	m_running = false;
	WHP_CORE_DEBUG("Window destroyed!");
	return true;
}

bool application::on_window_resize(window_resize_event& evnt)
{
	if (evnt.get_width() == 0 || evnt.get_height() == 0)
	{
		m_minimized = true;
		return false;
	}
	m_minimized = false;
	renderer::on_window_resize(evnt.get_width(), evnt.get_height());

	return false;
}

_WHIP_END