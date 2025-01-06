#pragma once

#include "Window.h"
#include "Log.h"
#include "memory.h"
#include "Timestep.h"
#include <Whip/Events/ApplicationEvent.h>
#include <Whip/Core/LayerStack.h>
#include <Whip/ImGui/ImGuiLayer.h>

#include <vector>
#include <functional>
#include <thread>
#include <mutex>

int main(int argc, char** argv);

_WHIP_START

enum class application_mode { editor = 0, runtime };

struct application_command_line_args
{
	int count = 0;
	char** args = nullptr;

	const char* operator[](int index) const
	{
		WHP_CORE_ASSERT(index < count, "[Application] arg index is out of the range!");
		return args[index];
	}
};

struct application_specification
{
	window_props properties;
	std::string working_directory;
	application_command_line_args command_line_args;
};

class application
{
public:
	application(const application_specification& spec);
	virtual ~application();

	void close();
	void restart();
	void on_event(event& evnt);
	void push_layer(layerptr layer);
	void push_overlay(layerptr overlay);

	WHP_NODISCARD static application& get() { return DREF(s_instance); }
	WHP_NODISCARD window& get_window() { return DREF(m_window); }
	WHP_NODISCARD imgui_layer* get_imgui_layer() { return m_imgui_layer; }
	WHP_NODISCARD application_specification get_specification() const { return m_specification; }
	WHP_NODISCARD uint64_t get_tick_count() const { return m_tick_count; }
	WHP_NODISCARD std::thread::id get_main_thread_id() const { return m_main_thread_id; }
	WHP_NODISCARD bool is_main_thread() const { return std::this_thread::get_id() == m_main_thread_id; }

	void submit_to_main_thread(const std::function<void()>& function);
	void submit_to_next_tick(const std::function<void()>& function);
private:
	void run();

	bool on_window_close(window_close_event& evnt);
	bool on_window_resize(window_resize_event& evnt);

	void execute_main_thread_queue();
	void execute_next_tick_queue();
private:
	static application* s_instance;
private:
	application_specification m_specification;
	scope<window> m_window;
	imgui_layer* m_imgui_layer;
	layer_stack m_layer_stack;
	bool m_running = true;
	bool m_minimized = false;
	float m_last_frame_time = 0.0f;
	uint64_t m_tick_count = 0;
	std::thread::id m_main_thread_id;

	std::vector<std::function<void()>> m_main_thread_queue;
	std::vector<std::function<void()>> m_next_tick_queue;
	std::mutex m_main_thread_queue_mutex;

	friend int ::main(int argc, char** argv);
};

// to be defined in client
application* create_application(application_command_line_args args);

_WHIP_END
