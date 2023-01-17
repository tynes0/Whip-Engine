#include <whippch.h>
#include <Platform/Windows/WindowsInput.h>
#include <Whip/Application.h>

#include <GLFW/glfw3.h>

_WHIP_START

Input* Input::s_Instance = new WindowsInput();

bool WindowsInput::isKeyPressedImpl(int keycode)
{
	auto window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
	int state = glfwGetKey(window, keycode);
	bool isPressed = ((state == GLFW_PRESS) || (state == GLFW_REPEAT));
	return isPressed;
}

bool WindowsInput::isMouseButtonPressedImpl(int button)
{
	auto window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
	int state = glfwGetMouseButton(window, button);
	bool isPressed = (state == GLFW_PRESS);
	return isPressed;
}

float WindowsInput::getMousePosXImpl()
{
	auto [PosX, PosY] = getMousePosImpl();
	return PosX;
}

float WindowsInput::getMousePosYImpl()
{
	auto[PosX, PosY] = getMousePosImpl();
	return PosY;
}

std::pair<float, float> WindowsInput::getMousePosImpl()
{
	auto window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
	double PosX, PosY;
	glfwGetCursorPos(window, &PosX, &PosY);
	return { PosX, PosY };
}

_WHIP_END