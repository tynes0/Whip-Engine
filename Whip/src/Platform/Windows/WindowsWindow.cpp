#include <whippch.h>
#include "WindowsWindow.h"

#include <Whip/Events/Event Callback Functions/EventCallbackFunctions.h>
#include <Platform/OpenGL/OpenGLContext.h>

_WHIP_START

static bool s_GLFWInitialized = false;

#ifdef WHP_PLATFORM_WINDOWS

WHP_NODISCARD Window* Window::create(const window_props& props)
{
	return new WindowsWindow(props);
}

#endif // WHP_PLATFORM_WINDOWS

WindowsWindow::WindowsWindow(const window_props& props)
{
	init(props);
}

WindowsWindow::~WindowsWindow()
{
	shutdown();
}

void WindowsWindow::init(const window_props& props)
{
	m_data.win_props = props;

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
	m_window = glfwCreateWindow((int)props.Width, (int)props.Height, m_data.win_props.Title.c_str(), nullptr, nullptr);
	m_context = new OpenGLContext(m_window);
	m_context->Init();

	glfwSetWindowUserPointer(m_window, &m_data);
	set_vsync(true);

	// Set GLFW callbacks
	glfwSetWindowSizeCallback(m_window, WIN_RESIZE_EVENT_CALLBACK_E);
	glfwSetWindowCloseCallback(m_window, WIN_CLOSE_EVENT_CALLBACK_E);
	glfwSetKeyCallback(m_window, KEY_EVENT_CALLBACK_E);
	glfwSetCharCallback(m_window, KEY_TYPED_EVENT_CALLBACK_E);
	glfwSetMouseButtonCallback(m_window, MOUSE_BUTTON_EVENT_CALLBACK_E);
	glfwSetScrollCallback(m_window, MOUSE_SCROLL_EVENT_CALLBACK_E);
	glfwSetCursorPosCallback(m_window, MOUSE_MOVED_EVENT_CALLBACK_E);

	WHP_CORE_INFO("Created Window {0} ({1}, {2})", props.Title, props.Width, props.Height);
}

void WindowsWindow::shutdown()
{
	glfwDestroyWindow(m_window);
}

void WindowsWindow::OnUpdate()
{
	glfwPollEvents();
	m_context->SwapBuffers();
}

void WindowsWindow::set_vsync(bool enabled)
{
	if (enabled)
	{
		glfwSwapInterval(1);
	}
	else
	{
		glfwSwapInterval(0);
	}
	m_data.VSync = enabled;
}

WHP_NODISCARD bool WindowsWindow::is_vsync() const
{
	return m_data.VSync;
}

_WHIP_END