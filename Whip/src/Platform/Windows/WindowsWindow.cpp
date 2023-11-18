#include <whippch.h>
#include "WindowsWindow.h"

#include <Platform/OpenGL/OpenGLContext.h>

#include "Whip/Events/ApplicationEvent.h"
#include "Whip/Events/KeyEvent.h"
#include "Whip/Events/MouseEvent.h"

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
		glfwSetErrorCallback([](int err, const char* desc)->void
			{
				WHP_CORE_ERROR("GLFW Error ({0}): {1}", err, desc);
			});
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
	set_vsync(false);

	// Set GLFW callbacks
	glfwSetWindowSizeCallback(m_window, [](GLFWwindow* window, int width, int height)->void
		{
			window_data& data = DREF(window_data*)glfwGetWindowUserPointer(window);
			RESIZE_WIN_PROPS(data.win_props, width, height);
			window_resize_event evnt(width, height);
			data.event_callback(evnt);
		});

	glfwSetWindowCloseCallback(m_window, [](GLFWwindow* window)->void 
		{
			window_data& data = DREF(window_data*)glfwGetWindowUserPointer(window);
			window_close_event evnt;
			data.event_callback(evnt);
		});

	glfwSetKeyCallback(m_window, [](GLFWwindow * window, int key, int scanmode, int action, int mods)->void
		{
			window_data& data = DREF(window_data*)glfwGetWindowUserPointer(window);
			static repeat_t repeat_time = 1;
			switch (action)
			{
				case GLFW_RELEASE:
				{
					key_released_event evnt(key);
					data.event_callback(evnt);
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
		});

	glfwSetCharCallback(m_window, [](GLFWwindow* window, unsigned int keycode)->void
		{
			window_data& data = DREF(window_data*)glfwGetWindowUserPointer(window);
			key_typed_event evnt(keycode);
			data.event_callback(evnt);
		});

	glfwSetMouseButtonCallback(m_window, [](GLFWwindow* window, int button, int action, int mods)->void 
		{
			window_data& data = DREF(window_data*)glfwGetWindowUserPointer(window);
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
		});
	glfwSetScrollCallback(m_window, [](GLFWwindow* window, double OffsetX, double OffsetY)->void
		{
			window_data& data = DREF(window_data*)glfwGetWindowUserPointer(window);
			mouse_scrolled_event evnt((float)OffsetX, (float)OffsetY);
			data.event_callback(evnt);
		});
	glfwSetCursorPosCallback(m_window, [](GLFWwindow* window, double posX, double posY)->void
		{
			window_data& data = DREF(window_data*)glfwGetWindowUserPointer(window);
			mouse_moved_event evnt((float)posX, (float)posY);
			data.event_callback(evnt);
		});

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