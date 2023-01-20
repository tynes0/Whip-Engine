#include <whippch.h>
#include <Whip/Events/Event Callback Functions/EventCallbackFunctions.h>

#include <Platform/Windows/WindowsWindow.h>


_WHIP_START

#define GET_GLFW_WIN_PTR(ptr, w) DREF(ptr*)glfwGetWindowUserPointer(w)

repeat_t EventCallbackFunctions::repeat_time(1);
repeat_t EventCallbackFunctions::last_repeat_time(1);

void EventCallbackFunctions::WindowResizeEventCallback(GLFWwindow* window, int width, int height)
{
	WindowData& data = GET_GLFW_WIN_PTR(WindowData ,window);
	RESIZE_WIN_PROPS(data.WinProps, width, height);
	WindowResizeEvent event(width, height);
	data.EventCallback(event);
}

void EventCallbackFunctions::WindowCloseEventCallback(GLFWwindow* window)
{
	WindowData& data = GET_GLFW_WIN_PTR(WindowData, window);
	WindowCloseEvent event;
	data.EventCallback(event);
}

void EventCallbackFunctions::KeyPressedReleasedEventsCallback(GLFWwindow* window, int key, int scanmode, int action, int mods)
{
	WindowData& data = GET_GLFW_WIN_PTR(WindowData, window);
	switch (action)
	{
		case GLFW_RELEASE:
		{
			KeyReleasedEvent event(key);
			data.EventCallback(event);
			last_repeat_time = repeat_time;
			repeat_time = 1;
			break;
		}
		case GLFW_PRESS:
		{
			KeyPressedEvent event(key, 0);
			data.EventCallback(event);
			break;
		}

		case GLFW_REPEAT:
		{
			KeyPressedEvent event(key, repeat_time++);
			data.EventCallback(event);
			break;
		}
	}
}

void EventCallbackFunctions::MouseButtonPressedReleasedEventsCallback(GLFWwindow* window, int button, int action, int mods)
{
	WindowData& data = GET_GLFW_WIN_PTR(WindowData, window);
	switch (action)
	{
		case GLFW_RELEASE:
		{
			MouseButtonReleasedEvent event(button);
			data.EventCallback(event);
			break;
		}
		case GLFW_PRESS:
		{
			MouseButtonPressedEvent event(button);
			data.EventCallback(event);
			break;
		}
	}
}

void EventCallbackFunctions::KeyTypedEventCallback(GLFWwindow* window, unsigned int keycode)
{
	WindowData& data = GET_GLFW_WIN_PTR(WindowData, window);
	KeyTypedEvent event(keycode);
	data.EventCallback(event);
}

void EventCallbackFunctions::MouseScrollEventCallback(GLFWwindow* window, double OffsetX, double OffsetY)
{
	WindowData& data = GET_GLFW_WIN_PTR(WindowData, window);
	MouseScrolledEvent event((float)OffsetX, (float)OffsetY);
	data.EventCallback(event);
}

void EventCallbackFunctions::MouseMovedEventCallback(GLFWwindow* window, double posX, double posY)
{
	WindowData& data = GET_GLFW_WIN_PTR(WindowData, window);
	MouseMovedEvent event((float)posX, (float)posY);
	data.EventCallback(event);
}

void EventCallbackFunctions::GLFWErrorCallback(int err, const char* desc)
{
	WHP_CORE_ERROR("GLFW Error ({0}): {1}", err, desc);
}

_WHIP_END