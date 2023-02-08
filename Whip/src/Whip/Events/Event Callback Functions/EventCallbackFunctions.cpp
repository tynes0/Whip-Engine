#include <whippch.h>
#include <Whip/Events/Event Callback Functions/EventCallbackFunctions.h>

#include <Platform/Windows/WindowsWindow.h>


_WHIP_START

#define GET_GLFW_WIN_USER_PTR(ptr, w) DREF(ptr*)glfwGetWindowUserPointer(w)

repeat_t event_callback_functions::repeat_time(1);
repeat_t event_callback_functions::last_repeat_time(1);

void event_callback_functions::window_resize_event_callback(GLFWwindow* window, int width, int height)
{
	window_data& data = GET_GLFW_WIN_USER_PTR(window_data,window);
	RESIZE_WIN_PROPS(data.win_props, width, height);
	window_resize_event evnt(width, height);
	data.event_callback(evnt);
}

void event_callback_functions::window_close_event_callback(GLFWwindow* window)
{
	window_data& data = GET_GLFW_WIN_USER_PTR(window_data, window);
	window_close_event evnt;
	data.event_callback(evnt);
}

void event_callback_functions::key_pressed_released_events_callback(GLFWwindow* window, int key, int scanmode, int action, int mods)
{
	window_data& data = GET_GLFW_WIN_USER_PTR(window_data, window);
	switch (action)
	{
		case GLFW_RELEASE:
		{
			key_released_event evnt(key);
			data.event_callback(evnt);
			last_repeat_time = repeat_time;
			repeat_time = 1;
			break;
		}
		case GLFW_PRESS:
		{
			key_pressed_event evnt(key, 0);
			data.event_callback(evnt);
			break;
		}

		case GLFW_REPEAT:
		{
			key_pressed_event evnt(key, repeat_time++);
			data.event_callback(evnt);
			break;
		}
	}
}

void event_callback_functions::mouse_button_pressed_released_events_callback(GLFWwindow* window, int button, int action, int mods)
{
	window_data& data = GET_GLFW_WIN_USER_PTR(window_data, window);
	switch (action)
	{
		case GLFW_RELEASE:
		{
			mouse_button_released_event evnt(button);
			data.event_callback(evnt);
			break;
		}
		case GLFW_PRESS:
		{
			mouse_button_pressed_event evnt(button);
			data.event_callback(evnt);
			break;
		}
	}
}

void event_callback_functions::key_typed_ecent_callback(GLFWwindow* window, unsigned int keycode)
{
	window_data& data = GET_GLFW_WIN_USER_PTR(window_data, window);
	key_typed_event evnt(keycode);
	data.event_callback(evnt);
}

void event_callback_functions::mouse_scroll_event_callback(GLFWwindow* window, double OffsetX, double OffsetY)
{
	window_data& data = GET_GLFW_WIN_USER_PTR(window_data, window);
	mouse_scrolled_event evnt((float)OffsetX, (float)OffsetY);
	data.event_callback(evnt);
}

void event_callback_functions::mouse_moved_event_callback(GLFWwindow* window, double posX, double posY)
{
	window_data& data = GET_GLFW_WIN_USER_PTR(window_data, window);
	mouse_moved_event evnt((float)posX, (float)posY);
	data.event_callback(evnt);
}

void event_callback_functions::glfw_error_callback(int err, const char* desc)
{
	WHP_CORE_ERROR("GLFW Error ({0}): {1}", err, desc);
}

_WHIP_END