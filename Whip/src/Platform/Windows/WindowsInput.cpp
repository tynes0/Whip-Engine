#include <WhipPch.h>
#include <Whip/Core/Input.h>
#include <Whip/Core/Application.h>

#include <GLFW/glfw3.h>

_WHIP_START

struct InputData
{
	static constexpr size_t MaxKeys = 512;
	static constexpr size_t MaxButtons = 8;
	std::bitset<MaxKeys> m_PreviousKeyStates;
	std::bitset<MaxButtons> m_PreviousMouseButtonStates;
};

static InputData s_InputData;

WHP_NODISCARD bool Input::IsKeyPressed(int keyCode)
{
	auto window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
	int state = glfwGetKey(window, keyCode);
	bool isPressed = (state == GLFW_PRESS);

	// Return true only on the first frame the key is pressed
	if (isPressed && !s_InputData.m_PreviousKeyStates[keyCode])
	{
		s_InputData.m_PreviousKeyStates[keyCode] = true;
		return true;
	}

	// Keep the current key state while suppressing repeated pressed checks
	s_InputData.m_PreviousKeyStates[keyCode] = isPressed;
	return false;
}

WHP_NODISCARD bool Input::IsKeyReleased(int keyCode)
{
	auto window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
	int state = glfwGetKey(window, keyCode);
	bool isReleased = (state == GLFW_RELEASE);

	// Return true only on the first frame the key is released
	if (isReleased && s_InputData.m_PreviousKeyStates[keyCode])
	{
		s_InputData.m_PreviousKeyStates[keyCode] = false;
		return true;
	}

	// Keep the current key state while suppressing repeated released checks
	s_InputData.m_PreviousKeyStates[keyCode] = !isReleased;
	return false;
}


WHP_NODISCARD bool Input::IsKeyDown(int keyCode)
{
	auto window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
	int state = glfwGetKey(window, keyCode);
	bool isPressed = (state == GLFW_PRESS);
	return isPressed;
}

bool Input::IsKeyUp(int keyCode)
{
	auto window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
	int state = glfwGetKey(window, keyCode);
	bool isReleased = (state == GLFW_RELEASE);
	return isReleased;
}

WHP_NODISCARD bool Input::IsMouseButtonPressed(int button)
{
	auto window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
	int state = glfwGetMouseButton(window, button);
	bool isPressed = (state == GLFW_PRESS);
	if (isPressed && !s_InputData.m_PreviousMouseButtonStates[button])
	{
		s_InputData.m_PreviousMouseButtonStates[button] = true;
		return true;
	}
	return false;
}

WHP_NODISCARD bool Input::IsMouseButtonReleased(int button)
{
	auto window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
	int state = glfwGetMouseButton(window, button);
	bool isReleased = (state == GLFW_RELEASE);
	if (isReleased && s_InputData.m_PreviousMouseButtonStates[button])
	{
		s_InputData.m_PreviousMouseButtonStates[button] = false;
		return true;
	}
	return false;
}

WHP_NODISCARD bool Input::IsMouseButtonDown(int button)
{
	auto window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
	int state = glfwGetMouseButton(window, button);
	bool isPressed = (state == GLFW_PRESS);
	return isPressed;
}

WHP_NODISCARD bool Input::IsMouseButtonUp(int button)
{
	auto window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
	int state = glfwGetMouseButton(window, button);
	bool isReleased = (state == GLFW_RELEASE);
	return isReleased;
}

WHP_NODISCARD float Input::GetMouseX()
{
	auto posX = GetMousePosition().first;
	return posX;
}

WHP_NODISCARD float Input::GetMouseY()
{
	auto posY = GetMousePosition().second;
	return posY;
}

WHP_NODISCARD std::pair<float, float> Input::GetMousePosition()
{
	auto window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
	double posX, posY;
	glfwGetCursorPos(window, &posX, &posY);
	return { (float)posX, (float)posY };
}

WHP_NODISCARD float Input::GetScrollDelta()
{
	return Application::Get().GetWindow().GetScrollDelta();
}


_WHIP_END
