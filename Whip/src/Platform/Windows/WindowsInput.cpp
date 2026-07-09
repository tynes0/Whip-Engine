#include <WhipPch.h>
#include <Whip/Core/Input.h>
#include <Whip/Core/Application.h>

#include <GLFW/glfw3.h>

_WHIP_START

namespace
{
	struct InputData
	{
		static constexpr size_t MaxKeys = 512;
		static constexpr size_t MaxButtons = 8;

		std::bitset<MaxKeys> m_PreviousKeyStates;
		std::bitset<MaxKeys> m_CurrentKeyStates;
		std::bitset<MaxButtons> m_PreviousMouseButtonStates;
		std::bitset<MaxButtons> m_CurrentMouseButtonStates;

		glm::vec2 m_PreviousMousePosition{ 0.0f };
		glm::vec2 m_CurrentMousePosition{ 0.0f };
		glm::vec2 m_MouseDelta{ 0.0f };
		glm::vec2 m_ViewportMin{ 0.0f };
		glm::vec2 m_ViewportMax{ 0.0f };

		float m_ScrollDeltaX = 0.0f;
		float m_ScrollDeltaY = 0.0f;
		bool m_MouseSampled = false;
		bool m_ViewportHovered = true;
		bool m_ViewportFocused = true;
		bool m_RuntimeInputEnabled = true;
		bool m_RuntimeInputCapturedByUI = false;
	};

	static constexpr KeyCode s_ValidKeys[] =
	{
		Key::Space, Key::Apostrophe, Key::Comma, Key::Minus, Key::Period, Key::Slash,
		Key::D0, Key::D1, Key::D2, Key::D3, Key::D4, Key::D5, Key::D6, Key::D7, Key::D8, Key::D9,
		Key::Semicolon, Key::Equal,
		Key::A, Key::B, Key::C, Key::D, Key::E, Key::F, Key::G, Key::H, Key::I, Key::J, Key::K, Key::L, Key::M,
		Key::N, Key::O, Key::P, Key::Q, Key::R, Key::S, Key::T, Key::U, Key::V, Key::W, Key::X, Key::Y, Key::Z,
		Key::LeftBracket, Key::Backslash, Key::RightBracket, Key::GraveAccent, Key::World1, Key::World2,
		Key::Escape, Key::Enter, Key::Tab, Key::Backspace, Key::Insert, Key::Delete,
		Key::Right, Key::Left, Key::Down, Key::Up, Key::PageUp, Key::PageDown, Key::Home, Key::End,
		Key::CapsLock, Key::ScrollLock, Key::NumLock, Key::PrintScreen, Key::Pause,
		Key::F1, Key::F2, Key::F3, Key::F4, Key::F5, Key::F6, Key::F7, Key::F8, Key::F9, Key::F10, Key::F11, Key::F12,
		Key::F13, Key::F14, Key::F15, Key::F16, Key::F17, Key::F18, Key::F19, Key::F20, Key::F21, Key::F22, Key::F23, Key::F24, Key::F25,
		Key::KP0, Key::KP1, Key::KP2, Key::KP3, Key::KP4, Key::KP5, Key::KP6, Key::KP7, Key::KP8, Key::KP9,
		Key::KPDecimal, Key::KPDivide, Key::KPMultiply, Key::KPSubtract, Key::KPAdd, Key::KPEnter, Key::KPEqual,
		Key::LeftShift, Key::LeftControl, Key::LeftAlt, Key::LeftSuper,
		Key::RightShift, Key::RightControl, Key::RightAlt, Key::RightSuper, Key::Menu
	};

	static InputData s_InputData;

	GLFWwindow* GetGLFWWindow()
	{
		return static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
	}

	bool IsValidKey(int keyCode)
	{
		return keyCode >= 0 && static_cast<size_t>(keyCode) < InputData::MaxKeys;
	}

	bool IsValidButton(int button)
	{
		return button >= 0 && static_cast<size_t>(button) < InputData::MaxButtons;
	}
}

void Input::BeginFrame()
{
	GLFWwindow* window = GetGLFWWindow();
	if (!window)
		return;

	s_InputData.m_PreviousKeyStates = s_InputData.m_CurrentKeyStates;
	s_InputData.m_CurrentKeyStates.reset();
	for (KeyCode key : s_ValidKeys)
	{
		const int state = glfwGetKey(window, static_cast<int>(key));
		s_InputData.m_CurrentKeyStates[key] = state == GLFW_PRESS || state == GLFW_REPEAT;
	}

	s_InputData.m_PreviousMouseButtonStates = s_InputData.m_CurrentMouseButtonStates;
	for (size_t button = 0; button < InputData::MaxButtons; ++button)
	{
		const int state = glfwGetMouseButton(window, static_cast<int>(button));
		s_InputData.m_CurrentMouseButtonStates[button] = state == GLFW_PRESS;
	}

	double posX = 0.0;
	double posY = 0.0;
	glfwGetCursorPos(window, &posX, &posY);
	const glm::vec2 position{ static_cast<float>(posX), static_cast<float>(posY) };
	if (!s_InputData.m_MouseSampled)
	{
		s_InputData.m_PreviousMousePosition = position;
		s_InputData.m_MouseSampled = true;
	}
	else
	{
		s_InputData.m_PreviousMousePosition = s_InputData.m_CurrentMousePosition;
	}

	s_InputData.m_CurrentMousePosition = position;
	s_InputData.m_MouseDelta = s_InputData.m_CurrentMousePosition - s_InputData.m_PreviousMousePosition;
	s_InputData.m_ScrollDeltaX = Application::Get().GetWindow().GetScrollDeltaX();
	s_InputData.m_ScrollDeltaY = Application::Get().GetWindow().GetScrollDeltaY();
	s_InputData.m_RuntimeInputCapturedByUI = false;
}

WHP_NODISCARD bool Input::IsKeyPressed(int keyCode)
{
	if (!IsValidKey(keyCode))
		return false;
	return s_InputData.m_CurrentKeyStates[keyCode] && !s_InputData.m_PreviousKeyStates[keyCode];
}

WHP_NODISCARD bool Input::IsKeyReleased(int keyCode)
{
	if (!IsValidKey(keyCode))
		return false;
	return !s_InputData.m_CurrentKeyStates[keyCode] && s_InputData.m_PreviousKeyStates[keyCode];
}

WHP_NODISCARD bool Input::IsKeyDown(int keyCode)
{
	if (!IsValidKey(keyCode))
		return false;
	return s_InputData.m_CurrentKeyStates[keyCode];
}

bool Input::IsKeyUp(int keyCode)
{
	if (!IsValidKey(keyCode))
		return true;
	return !s_InputData.m_CurrentKeyStates[keyCode];
}

WHP_NODISCARD bool Input::IsMouseButtonPressed(int button)
{
	if (!IsValidButton(button))
		return false;
	return s_InputData.m_CurrentMouseButtonStates[button] && !s_InputData.m_PreviousMouseButtonStates[button];
}

WHP_NODISCARD bool Input::IsMouseButtonReleased(int button)
{
	if (!IsValidButton(button))
		return false;
	return !s_InputData.m_CurrentMouseButtonStates[button] && s_InputData.m_PreviousMouseButtonStates[button];
}

WHP_NODISCARD bool Input::IsMouseButtonDown(int button)
{
	if (!IsValidButton(button))
		return false;
	return s_InputData.m_CurrentMouseButtonStates[button];
}

WHP_NODISCARD bool Input::IsMouseButtonUp(int button)
{
	if (!IsValidButton(button))
		return true;
	return !s_InputData.m_CurrentMouseButtonStates[button];
}

WHP_NODISCARD float Input::GetMouseX()
{
	return s_InputData.m_CurrentMousePosition.x;
}

WHP_NODISCARD float Input::GetMouseY()
{
	return s_InputData.m_CurrentMousePosition.y;
}

WHP_NODISCARD std::pair<float, float> Input::GetMousePosition()
{
	return { s_InputData.m_CurrentMousePosition.x, s_InputData.m_CurrentMousePosition.y };
}

WHP_NODISCARD glm::vec2 Input::GetMouseDelta()
{
	return s_InputData.m_MouseDelta;
}

WHP_NODISCARD float Input::GetMouseDeltaX()
{
	return s_InputData.m_MouseDelta.x;
}

WHP_NODISCARD float Input::GetMouseDeltaY()
{
	return s_InputData.m_MouseDelta.y;
}

WHP_NODISCARD glm::vec2 Input::GetMouseViewportPosition()
{
	return s_InputData.m_CurrentMousePosition - s_InputData.m_ViewportMin;
}

WHP_NODISCARD bool Input::IsMouseInsideViewport()
{
	const glm::vec2& mouse = s_InputData.m_CurrentMousePosition;
	return mouse.x >= s_InputData.m_ViewportMin.x &&
		mouse.y >= s_InputData.m_ViewportMin.y &&
		mouse.x <= s_InputData.m_ViewportMax.x &&
		mouse.y <= s_InputData.m_ViewportMax.y;
}

WHP_NODISCARD float Input::GetScrollDelta()
{
	return s_InputData.m_ScrollDeltaY;
}

WHP_NODISCARD float Input::GetScrollDeltaX()
{
	return s_InputData.m_ScrollDeltaX;
}

WHP_NODISCARD float Input::GetScrollDeltaY()
{
	return s_InputData.m_ScrollDeltaY;
}

void Input::SetViewportState(bool hovered, bool focused, const glm::vec2& min, const glm::vec2& max)
{
	s_InputData.m_ViewportHovered = hovered;
	s_InputData.m_ViewportFocused = focused;
	s_InputData.m_ViewportMin = min;
	s_InputData.m_ViewportMax = max;
}

WHP_NODISCARD bool Input::IsViewportHovered()
{
	return s_InputData.m_ViewportHovered;
}

WHP_NODISCARD bool Input::IsViewportFocused()
{
	return s_InputData.m_ViewportFocused;
}

void Input::SetRuntimeInputEnabled(bool enabled)
{
	s_InputData.m_RuntimeInputEnabled = enabled;
}

WHP_NODISCARD bool Input::IsRuntimeInputEnabled()
{
	return s_InputData.m_RuntimeInputEnabled;
}

WHP_NODISCARD bool Input::IsRuntimeInputActive()
{
	if (Application::Get().GetMode() == ApplicationMode::Runtime)
		return s_InputData.m_RuntimeInputEnabled;
	return s_InputData.m_RuntimeInputEnabled && s_InputData.m_ViewportHovered && s_InputData.m_ViewportFocused;
}

void Input::SetRuntimeInputCapturedByUI(bool captured)
{
	s_InputData.m_RuntimeInputCapturedByUI = captured;
}

WHP_NODISCARD bool Input::IsRuntimeInputCapturedByUI()
{
	return s_InputData.m_RuntimeInputCapturedByUI;
}

WHP_NODISCARD bool Input::IsRuntimeGameplayInputActive()
{
	return IsRuntimeInputActive() && !s_InputData.m_RuntimeInputCapturedByUI;
}

void Input::SetCursorMode(CursorMode mode)
{
	Application::Get().GetWindow().SetCursorMode(mode);
}

WHP_NODISCARD CursorMode Input::GetCursorMode()
{
	return Application::Get().GetWindow().GetCursorMode();
}

void Input::SetCursorVisible(bool visible)
{
	SetCursorMode(visible ? CursorMode::Normal : CursorMode::Hidden);
}

WHP_NODISCARD bool Input::IsCursorVisible()
{
	return GetCursorMode() == CursorMode::Normal;
}

void Input::SetCursorShape(CursorShape shape)
{
	Application::Get().GetWindow().SetCursorShape(shape);
}

WHP_NODISCARD CursorShape Input::GetCursorShape()
{
	return Application::Get().GetWindow().GetCursorShape();
}

_WHIP_END
