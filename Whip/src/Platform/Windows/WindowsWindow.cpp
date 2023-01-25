#include <whippch.h>
#include "WindowsWindow.h"

#include <Whip/Events/Event Callback Functions/EventCallbackFunctions.h>
#include <Platform/OpenGL/OpenGLContext.h>

_WHIP_START

static bool s_GLFWInitialized = false;

#ifdef WHP_PLATFORM_WINDOWS

WHP_NODISCARD Window* Window::Create(const WindowProps& props)
{
	return new WindowsWindow(props);
}

#endif // WHP_PLATFORM_WINDOWS

WindowsWindow::WindowsWindow(const WindowProps& props)
{
	Init(props);
}

WindowsWindow::~WindowsWindow()
{
	Shutdown();
}

void WindowsWindow::Init(const WindowProps& props)
{
	m_Data.WinProps = props;

	WHP_CORE_INFO("Creating Window {0} ({1}, {2})", props.Title, props.Width, props.Height);

	// initialize GLFW
	if (!s_GLFWInitialized)
	{
		int success = glfwInit();
		WHP_CORE_ASSERT(success, "Could not initialize GLFW!");
		glfwSetErrorCallback(GLFW_ERR_CALLBACK_E);
		s_GLFWInitialized = true;
	}

	// create window
	m_Window = glfwCreateWindow((int)props.Width, (int)props.Height, m_Data.WinProps.Title.c_str(), nullptr, nullptr);
	m_Context = new OpenGLContext(m_Window);
	m_Context->Init();

	glfwSetWindowUserPointer(m_Window, &m_Data);
	SetVSync(true);

	// Set GLFW callbacks
	glfwSetWindowSizeCallback(m_Window, WIN_RESIZE_EVENT_CALLBACK_E);
	glfwSetWindowCloseCallback(m_Window, WIN_CLOSE_EVENT_CALLBACK_E);
	glfwSetKeyCallback(m_Window, KEY_EVENT_CALLBACK_E);
	glfwSetCharCallback(m_Window, KEY_TYPED_EVENT_CALLBACK_E);
	glfwSetMouseButtonCallback(m_Window, MOUSE_BUTTON_EVENT_CALLBACK_E);
	glfwSetScrollCallback(m_Window, MOUSE_SCROLL_EVENT_CALLBACK_E);
	glfwSetCursorPosCallback(m_Window, MOUSE_MOVED_EVENT_CALLBACK_E);

	WHP_CORE_INFO("Created Window {0} ({1}, {2})", props.Title, props.Width, props.Height);
}

void WindowsWindow::Shutdown()
{
	glfwDestroyWindow(m_Window);
}

void WindowsWindow::OnUpdate()
{
	glfwPollEvents();
	m_Context->SwapBuffers();
}

void WindowsWindow::SetVSync(bool enabled)
{
	if (enabled)
	{
		glfwSwapInterval(1);
	}
	else
	{
		glfwSwapInterval(0);
	}
	m_Data.VSync = enabled;
}

WHP_NODISCARD bool WindowsWindow::IsVSync() const
{
	return m_Data.VSync;
}

_WHIP_END