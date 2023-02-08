#pragma once

#include <GLFW/glfw3.h>

#include <Whip/Events/ApplicationEvent.h>
#include <Whip/Events/KeyEvent.h>
#include <Whip/Events/MouseEvent.h>

_WHIP_START

class WHIP_API event_callback_functions
{
private:
	static repeat_t repeat_time;
	static repeat_t last_repeat_time;
public:
	// get repeat time
	WHP_NODISCARD inline static repeat_t get_last_repeat_time() { return last_repeat_time; }

	static void window_resize_event_callback(GLFWwindow* window, int width, int height);
	static void window_close_event_callback(GLFWwindow* window);
	static void key_pressed_released_events_callback(GLFWwindow* window, int key, int scanmode, int action, int mods);
	static void key_typed_ecent_callback(GLFWwindow* window, unsigned int keycode);
	static void mouse_button_pressed_released_events_callback(GLFWwindow* window, int button, int action, int mods);
	static void mouse_scroll_event_callback(GLFWwindow* window, double OffsetX, double OffsetY);
	static void mouse_moved_event_callback(GLFWwindow* window, double posX, double posY);
	static void glfw_error_callback(int err, const char* desc);
};


// macros for simple usage
#define WIN_RESIZE_EVENT_CALLBACK(window, width, height)				_WHIP event_callback_functions::window_resize_event_callback(window, width, height)
#define WIN_CLOSE_EVENT_CALLBACK(window)								_WHIP event_callback_functions::window_close_event_callback(window)
#define KEY_EVENT_CALLBACK(window, key, scanmode, action, mods)			_WHIP event_callback_functions::key_pressed_released_events_callback(window, key, scanmode, action, mods)
#define KEY_TYPED_EVENT_CALLBACK(window, key)							_WHIP event_callback_functions::key_typed_ecent_callback(window, key)
#define MOUSE_BUTTON_EVENT_CALLBACK(window, button, action, mods)		_WHIP event_callback_functions::mouse_button_pressed_released_events_callback(window, button, action, mods)
#define MOUSE_SCROLL_EVENT_CALLBACK(window, OffsetX, OffsetY)			_WHIP event_callback_functions::mouse_scroll_event_callback(window, OffsetX, OffsetY)
#define MOUSE_MOVED_EVENT_CALLBACK(window, PosX, PosY)					_WHIP event_callback_functions::mouse_moved_event_callback(window ,PosX, PosY)
#define GLFW_ERR_CALLBACK(err, desc)									_WHIP event_callback_functions::glfw_error_callback(err, desc)

// empty types
#define WIN_RESIZE_EVENT_CALLBACK_E						_WHIP event_callback_functions::window_resize_event_callback
#define WIN_CLOSE_EVENT_CALLBACK_E						_WHIP event_callback_functions::window_close_event_callback
#define KEY_EVENT_CALLBACK_E							_WHIP event_callback_functions::key_pressed_released_events_callback
#define KEY_TYPED_EVENT_CALLBACK_E						_WHIP event_callback_functions::key_typed_ecent_callback
#define MOUSE_BUTTON_EVENT_CALLBACK_E					_WHIP event_callback_functions::mouse_button_pressed_released_events_callback
#define MOUSE_SCROLL_EVENT_CALLBACK_E					_WHIP event_callback_functions::mouse_scroll_event_callback
#define MOUSE_MOVED_EVENT_CALLBACK_E					_WHIP event_callback_functions::mouse_moved_event_callback
#define GLFW_ERR_CALLBACK_E								_WHIP event_callback_functions::glfw_error_callback


_WHIP_END