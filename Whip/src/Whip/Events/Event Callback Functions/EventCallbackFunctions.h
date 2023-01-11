#pragma once

#include <GLFW/glfw3.h>

#include <Whip/Events/ApplicationEvent.h>
#include <Whip/Events/KeyEvent.h>
#include <Whip/Events/MouseEvent.h>

_WHIP_START

class WHIP_API EventCallbackFunctions
{
private:
	static int repeat_time;
public:
	static void WindowResizeEventCallback(GLFWwindow* window, int width, int height);
	static void WindowCloseEventCallback(GLFWwindow* window);
	static void KeyPressedReleasedEventsCallback(GLFWwindow* window, int key, int scanmode, int action, int mods);
	static void MouseButtonPressedReleasedEventsCallback(GLFWwindow* window, int button, int action, int mods);
	static void MouseScrollEventCallback(GLFWwindow* window, double OffsetX, double OffsetY);
	static void MouseMovedEventCallback(GLFWwindow* window, double posX, double posY);
	static void GLFWErrorCallback(int err, const char* desc);
};

_WHIP_END

// macros for simple usage
#define WIN_RESIZE_CALLBACK(window, width, height)						Whip::EventCallbackFunctions::WindowResizeEventCallback(window, width, height)
#define WIN_CLOSE_CALLBACK(window)										Whip::EventCallbackFunctions::WindowCloseEventCallback(window)
#define KEY_EVENT_CALLBACK(window, key, scanmode, action, mods)			Whip::EventCallbackFunctions::KeyPressedReleasedEventsCallback(window, key, scanmode, action, mods)
#define MOUSE_BUTTON_EVENT_CALLBACK(window, button, action, mods)		Whip::EventCallbackFunctions::MouseButtonPressedReleasedEventsCallback(window, button, action, mods)
#define MOUSE_SCROLL_EVENT_CALLBACK(window, OffsetX, OffsetY)			Whip::EventCallbackFunctions::MouseScrollEventCallback(window, OffsetX, OffsetY)
#define MOUSE_MOVED_EVENT_CALLBACK(window, PosX, PosY)					Whip::EventCallbackFunctions::MouseMovedEventCallback(window ,PosX, PosY)
#define GLFW_ERR_CALLBACK(err, desc)									Whip::EventCallbackFunctions::GLFWErrorCallback(err, desc)

// empty types
#define WIN_RESIZE_CALLBACK_E							Whip::EventCallbackFunctions::WindowResizeEventCallback
#define WIN_CLOSE_CALLBACK_E							Whip::EventCallbackFunctions::WindowCloseEventCallback
#define KEY_EVENT_CALLBACK_E							Whip::EventCallbackFunctions::KeyPressedReleasedEventsCallback
#define MOUSE_BUTTON_EVENT_CALLBACK_E					Whip::EventCallbackFunctions::MouseButtonPressedReleasedEventsCallback
#define MOUSE_SCROLL_EVENT_CALLBACK_E					Whip::EventCallbackFunctions::MouseScrollEventCallback
#define MOUSE_MOVED_EVENT_CALLBACK_E					Whip::EventCallbackFunctions::MouseMovedEventCallback
#define GLFW_ERR_CALLBACK_E								Whip::EventCallbackFunctions::GLFWErrorCallback