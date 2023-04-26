#include <whippch.h>
#include "WindowsWindow.h"

#include <Whip/Events/Event Callback Functions/EventCallbackFunctions.h>
#include <Platform/OpenGL/OpenGLContext.h>

_WHIP_START

static uint32_t s_GLFWwindow_count = 0;

#ifdef WHP_PLATFORM_WINDOWS

WHP_NODISCARD window* window::create(const window_props& props)
{
	return new windows_window(props);
}

#endif // WHP_PLATFORM_WINDOWS

windows_window::windows_window(const window_props& props)
{
	WHP_PROFILE_FUNCTION();

	init(props);
}

windows_window::~windows_window()
{
	WHP_PROFILE_FUNCTION();

	shutdown();
}

void windows_window::init(const window_props& props)
{
	WHP_PROFILE_FUNCTION();

	m_data.win_props = props;

	WHP_CORE_INFO("Creating Window {0} ({1}, {2})", props.m_title, props.m_width, props.m_height);

	// initialize GLFW
	if (s_GLFWwindow_count == 0)
	{
		WHP_PROFILE_SCOPE("glfwInit");
		int success = glfwInit();
		WHP_CORE_ASSERT(success, "Could not initialize GLFW!");
		glfwSetErrorCallback(GLFW_ERR_CALLBACK_E);
	}

	// create window
	{
		WHP_PROFILE_SCOPE("glfwCreateWindow");
		m_window = glfwCreateWindow((int)props.m_width, (int)props.m_height, m_data.win_props.m_title.c_str(), nullptr, nullptr);
		++s_GLFWwindow_count;
	}

	m_context = graphic_context::create(m_window);
	m_context->init();

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

	WHP_CORE_INFO("Created Window {0} ({1}, {2})", props.m_title, props.m_width, props.m_height);
}

void windows_window::shutdown()
{
	WHP_PROFILE_FUNCTION();

	glfwDestroyWindow(m_window);
}

void windows_window::on_update()
{
	WHP_PROFILE_FUNCTION();

	glfwPollEvents();
	m_context->swap_buffers();
}

void windows_window::set_vsync(bool enabled)
{
	WHP_PROFILE_FUNCTION();

	if (enabled)
	{
		glfwSwapInterval(1);
	}
	else
	{
		glfwSwapInterval(0);
	}
	m_data.vsync = enabled;
}

WHP_NODISCARD bool windows_window::is_vsync() const
{
	return m_data.vsync;
}

_WHIP_END