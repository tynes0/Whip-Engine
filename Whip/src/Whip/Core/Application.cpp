#include <whippch.h>
#include "Application.h"

#include <Whip/Render/Renderer.h>
#include <Whip/Utils/platform_utils.h>
#include <Whip/Scripting/script_engine.h>
#include <Whip/Audio/audio_engine.h>
#include <Whip/Core/timer_manager.h>

_WHIP_START

application* application::s_instance = nullptr;

application::application(const application_specification& spec)
	:m_specification(spec), m_main_thread_id(std::this_thread::get_id())
{
	WHP_PROFILE_FUNCTION();

	WHP_CORE_ASSERT(!s_instance, "Application already exist!");
	s_instance = this;

	if (!m_specification.working_directory.empty())
		std::filesystem::current_path(m_specification.working_directory);

	m_window = scope<window>(window::create(m_specification.properties));
	m_window->set_event_callback([this](auto&&... args) -> decltype(auto) { return this->on_event(std::forward<decltype(args)>(args)...); });
	
	m_window->set_vsync(false);
	renderer::init();
	audio_engine::init();

	m_imgui_layer = new imgui_layer();
	push_overlay(m_imgui_layer);
}

application::~application()
{
	WHP_PROFILE_FUNCTION();

	script_engine::shutdown();

	for (layerptr item : m_layer_stack)
		item->on_detach();
}

void application::run()
{
	WHP_PROFILE_FUNCTION();

	while (m_running)
	{
		m_tick_count++;
		execute_next_tick_queue();

		float l_time = time::get_time();
		timestep ts = l_time - m_last_frame_time;
		m_last_frame_time = l_time;
		execute_main_thread_queue();
		timer_manager::get().tick(ts);

		if (!m_minimized)
		{
			for (layerptr item : m_layer_stack)
			{
				item->on_update(ts);
			}
		}

		m_imgui_layer->begin();
		{
			for (layerptr item : m_layer_stack)
			{
				item->on_imgui_render();
			}
		}
		m_imgui_layer->end();
		m_window->on_update();
	}
}

void application::close()
{
	m_running = false;
}

void application::restart()
{
	utils::restart_program();
}

void application::on_event(event& e)
{
	WHP_PROFILE_FUNCTION();

	event_dispatcher dispatcher(e);
	dispatcher.dispatch<window_close_event>([this](auto&&... args) -> decltype(auto) { return this->on_window_close(std::forward<decltype(args)>(args)...); });
	dispatcher.dispatch<window_resize_event>([this](auto&&... args) -> decltype(auto) { return this->on_window_resize(std::forward<decltype(args)>(args)...); });

	for (auto iter = m_layer_stack.end(); iter != m_layer_stack.begin(); )
	{
		if (e.handled)
			break;
		(DREF(--iter))->on_event(e);
	}
}

void application::push_layer(layerptr layer)
{
	WHP_PROFILE_FUNCTION();

	m_layer_stack.push_layer(layer);
	layer->on_attach();
}

void application::push_overlay(layerptr overlay)
{
	WHP_PROFILE_FUNCTION();

	m_layer_stack.push_overlay(overlay);
	overlay->on_attach();
}

void application::submit_to_main_thread(const std::function<void()>& function)
{
	std::scoped_lock<std::mutex> lock(m_main_thread_queue_mutex);

	m_main_thread_queue.emplace_back(function);
}

void application::submit_to_next_tick(const std::function<void()>& function)
{
	m_next_tick_queue.emplace_back(function);
}

bool application::on_window_close(window_close_event& evnt)
{
	this->close();
	WHP_CORE_INFO("[Application] Window destroyed!");
	return true;
}

bool application::on_window_resize(window_resize_event& evnt)
{
	WHP_PROFILE_FUNCTION();

	if (evnt.get_width() == 0 || evnt.get_height() == 0)
	{
		m_minimized = true;
		return false;
	}
	m_minimized = false;
	renderer::on_window_resize(evnt.get_width(), evnt.get_height());

	return false;
}

void application::execute_main_thread_queue()
{
	std::scoped_lock<std::mutex> lock(m_main_thread_queue_mutex);

	for (auto& func : m_main_thread_queue)
		func();

	m_main_thread_queue.clear();
}

void application::execute_next_tick_queue()
{
	for (auto& func : m_next_tick_queue)
		func();
	m_next_tick_queue.clear();
}

_WHIP_END
