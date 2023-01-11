#include <whippch.h>
#include "EventCallbackFunctions.h"

#include <Platform/Windows/WindowsWindow.h>


_WHIP_START

#define GLFW_WIN_PTR *(WindowData*)glfwGetWindowUserPointer(window)

int EventCallbackFunctions::repeat_time = 1;

void EventCallbackFunctions::WindowResizeEventCallback(GLFWwindow* window, int width, int height)
{
	WindowData& data = GLFW_WIN_PTR;
	data.WinProps.Width = width;
	data.WinProps.Height = height;
	WindowResizeEvent event(width, height);
	data.EventCallback(event);
}

void EventCallbackFunctions::WindowCloseEventCallback(GLFWwindow* window)
{
	WindowData& data = GLFW_WIN_PTR;
	WindowCloseEvent event;
	data.EventCallback(event);
}

void EventCallbackFunctions::KeyPressedReleasedEventsCallback(GLFWwindow* window, int key, int scanmode, int action, int mods)
{
	WindowData& data = GLFW_WIN_PTR;
	switch (action)
	{
		case GLFW_RELEASE:
		{
			KeyReleasedEvent event(key);
			data.EventCallback(event);
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
	WindowData& data = GLFW_WIN_PTR;
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

void EventCallbackFunctions::MouseScrollEventCallback(GLFWwindow* window, double OffsetX, double OffsetY)
{
	WindowData& data = GLFW_WIN_PTR;
	MouseScrolledEvent event((float)OffsetX, (float)OffsetY);
	data.EventCallback(event);
}

void EventCallbackFunctions::MouseMovedEventCallback(GLFWwindow* window, double posX, double posY)
{
	WindowData& data = GLFW_WIN_PTR;
	MouseMovedEvent event((float)posX, (float)posY);
	data.EventCallback(event);
}

void EventCallbackFunctions::GLFWErrorCallback(int err, const char* desc)
{
	WHP_CORE_ERROR("GLFW Error ({0}): {1}", err, desc);
}

_WHIP_END