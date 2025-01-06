#include <whippch.h>
#include "WindowsWindow.h"

#include <Platform/OpenGL/OpenGLContext.h>

#include "Whip/Events/ApplicationEvent.h"
#include "Whip/Events/KeyEvent.h"
#include "Whip/Events/MouseEvent.h"

#include <Whip/Debug/Instrumentor.h>

_WHIP_START

static uint32_t s_GLFWwindow_count = 0;
static float s_scroll_delta = 0.0f;

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

	WHP_CORE_INFO("[Application] Creating Window {0} ({1}, {2})", props.title, props.width, props.height);

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

	// Birincil monitörü al
	GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();

	// Monitörün video modunu al
	const GLFWvidmode* mode = glfwGetVideoMode(primaryMonitor);

	glfwWindowHint(GLFW_MAXIMIZED, props.fullscreen);
	
	// create window
	{
		WHP_PROFILE_SCOPE("glfwCreateWindow");
		m_window = glfwCreateWindow((int)props.width, (int)props.height, m_data.win_props.title.c_str(), nullptr, nullptr);
		++s_GLFWwindow_count;
	}

	if (props.fullscreen)
		glfwGetWindowSize(m_window, (int*)(&m_data.win_props.width), (int*)(&m_data.win_props.height));
	

	m_context = graphic_context::create(m_window);
	m_context->init();

	glfwSetWindowUserPointer(m_window, &m_data);

	// Set GLFW callbacks
	glfwSetWindowSizeCallback(m_window, [](GLFWwindow* window, int width, int height)->void
		{
			window_data& data = DREF(window_data*)glfwGetWindowUserPointer(window);
			data.win_props.width = width;
			data.win_props.height = height;
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
					key_released_event evnt(static_cast<key_code>(key));
					data.event_callback(evnt);
					repeat_time = 1;
					break;
				}
				case GLFW_PRESS:
				{
					key_pressed_event evnt(static_cast<key_code>(key), 0);
					data.event_callback(evnt);
					break;
				}

				case GLFW_REPEAT:
				{
					key_pressed_event evnt(static_cast<key_code>(key), repeat_time++);
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
					mouse_button_released_event evnt(static_cast<mouse_code>(button));
					data.event_callback(evnt);
					break;
				}
				case GLFW_PRESS:
				{
					mouse_button_pressed_event evnt(static_cast<mouse_code>(button));
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
			s_scroll_delta = (float)OffsetY;
		});
	glfwSetCursorPosCallback(m_window, [](GLFWwindow* window, double posX, double posY)->void
		{
			window_data& data = DREF(window_data*)glfwGetWindowUserPointer(window);
			mouse_moved_event evnt((float)posX, (float)posY);
			data.event_callback(evnt);
		});

	glfwSetDropCallback(m_window, [](GLFWwindow* window, int pathCount, const char* paths[])
		{
			window_data& data = *(window_data*)glfwGetWindowUserPointer(window);

			std::vector<std::filesystem::path> filepaths(pathCount);
			for (int i = 0; i < pathCount; i++)
				filepaths[i] = paths[i];

			window_drop_event event(std::move(filepaths));
			data.event_callback(event);
		});

	WHP_CORE_INFO("[Application] Created Window {0} ({1}, {2})", props.title, props.width, props.height);
}

void windows_window::shutdown()
{
	WHP_PROFILE_FUNCTION();

	glfwDestroyWindow(m_window);
}

void windows_window::on_update()
{
	WHP_PROFILE_FUNCTION();
	s_scroll_delta = 0;
	glfwPollEvents();
	m_context->swap_buffers();
}

WHP_NODISCARD float windows_window::get_scroll_delta() const
{
	return s_scroll_delta;
}

std::pair<int, int> windows_window::get_position() const
{
	int x, y;
	glfwGetWindowPos(m_window, &x, &y);
	return { x, y };
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

bool windows_window::is_vsync() const
{
	return m_data.vsync;
}

_WHIP_END
