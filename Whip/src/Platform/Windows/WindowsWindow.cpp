#include <WhipPch.h>
#include "WindowsWindow.h"

#include <Platform/OpenGL/OpenGLContext.h>

#include <Whip/Events/ApplicationEvent.h>
#include <Whip/Events/KeyEvent.h>
#include <Whip/Events/MouseEvent.h>

#include <Whip/Debug/Instrumentor.h>

_WHIP_START

static uint32_t s_GLFWWindowCount = 0;
static float s_ScrollDelta = 0.0f;

#ifdef WHP_PLATFORM_WINDOWS

WHP_NODISCARD Window* Window::Create(const WindowProps& props)
{
	return new WindowsWindow(props);
}

#endif // WHP_PLATFORM_WINDOWS

WindowsWindow::WindowsWindow(const WindowProps& props)
{
	WHP_PROFILE_FUNCTION();

	Init(props);
}

WindowsWindow::~WindowsWindow()
{
	WHP_PROFILE_FUNCTION();

	Shutdown();
}

void WindowsWindow::Init(const WindowProps& props)
{
	WHP_PROFILE_FUNCTION();

	WHP_CORE_INFO("[Application] Creating Window {0} ({1}, {2})", props.m_Title, props.m_Width, props.m_Height);

	// initialize GLFW
	if (s_GLFWWindowCount == 0)
	{
		WHP_PROFILE_SCOPE("glfwInit");
		int success = glfwInit();
		WHP_CORE_ASSERT(success, "Could not initialize GLFW!");
		glfwSetErrorCallback([](int err, const char* desc)->void
			{
				WHP_CORE_ERROR("GLFW Error ({0}): {1}", err, desc);
			});
	}

	// Get primary monitor
	GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();

	// Get monitor video mode
	const GLFWvidmode* mode = glfwGetVideoMode(primaryMonitor);

	glfwWindowHint(GLFW_MAXIMIZED, props.m_Fullscreen);
	
	// create Window
	{
		WHP_PROFILE_SCOPE("glfwCreateWindow");
		m_Data.m_Properties = props;
		m_Window = glfwCreateWindow((int)props.m_Width, (int)props.m_Height, m_Data.m_Properties.m_Title.c_str(), nullptr, nullptr);
		++s_GLFWWindowCount;
	}

	if (props.m_Fullscreen)
		glfwGetWindowSize(m_Window, (int*)(&m_Data.m_Properties.m_Width), (int*)(&m_Data.m_Properties.m_Height));
	

	m_Context = GraphicContext::Create(m_Window);
	m_Context->Init();

	glfwSetWindowUserPointer(m_Window, &m_Data);

	// Set GLFW callbacks
	glfwSetWindowSizeCallback(m_Window, [](GLFWwindow* window, int width, int height)->void
		{
			WindowData& data = DREF(WindowData*)glfwGetWindowUserPointer(window);
			data.m_Properties.m_Width = width;
			data.m_Properties.m_Height = height;
			WindowResizeEvent event(width, height);
			data.m_EventCallback(event);
		});

	glfwSetWindowCloseCallback(m_Window, [](GLFWwindow* window)->void 
		{
			WindowData& data = DREF(WindowData*)glfwGetWindowUserPointer(window);
			WindowCloseEvent event;
			data.m_EventCallback(event);
		});

	glfwSetKeyCallback(m_Window, [](GLFWwindow * window, int key, int scanmode, int action, int mods)->void
		{
			WindowData& data = DREF(WindowData*)glfwGetWindowUserPointer(window);
			static RepeatType repeatTime = 1;
			switch (action)
			{
				case GLFW_RELEASE:
				{
					KeyReleasedEvent event(static_cast<KeyCode>(key));
					data.m_EventCallback(event);
					repeatTime = 1;
					break;
				}
				case GLFW_PRESS:
				{
					KeyPressedEvent event(static_cast<KeyCode>(key), 0);
					data.m_EventCallback(event);
					break;
				}

				case GLFW_REPEAT:
				{
					KeyPressedEvent event(static_cast<KeyCode>(key), repeatTime++);
					data.m_EventCallback(event);
					break;
				}
			}
		});

	glfwSetCharCallback(m_Window, [](GLFWwindow* window, unsigned int keyCode)->void
	{
		WindowData& data = DREF(WindowData*)glfwGetWindowUserPointer(window);
		KeyTypedEvent event(keyCode);
		data.m_EventCallback(event);
	});

	glfwSetMouseButtonCallback(m_Window, [](GLFWwindow* window, int button, int action, int mods)->void 
		{
			WindowData& data = DREF(WindowData*)glfwGetWindowUserPointer(window);
			switch (action)
			{
				case GLFW_RELEASE:
				{
					MouseButtonReleasedEvent event(static_cast<MouseCode>(button));
					data.m_EventCallback(event);
					break;
				}
				case GLFW_PRESS:
				{
					MouseButtonPressedEvent event(static_cast<MouseCode>(button));
					data.m_EventCallback(event);
					break;
				}
			}
		});
	glfwSetScrollCallback(m_Window, [](GLFWwindow* window, double offsetX, double offsetY)->void
		{
			WindowData& data = DREF(WindowData*)glfwGetWindowUserPointer(window);
			MouseScrolledEvent event((float)offsetX, (float)offsetY);
			data.m_EventCallback(event);
			s_ScrollDelta = (float)offsetY;
		});
	glfwSetCursorPosCallback(m_Window, [](GLFWwindow* window, double posX, double posY)->void
		{
			WindowData& data = DREF(WindowData*)glfwGetWindowUserPointer(window);
			MouseMovedEvent event((float)posX, (float)posY);
			data.m_EventCallback(event);
		});

	glfwSetDropCallback(m_Window, [](GLFWwindow* window, int pathCount, const char* paths[])
		{
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

			std::vector<std::filesystem::path> filepaths(pathCount);
			for (int i = 0; i < pathCount; i++)
				filepaths[i] = paths[i];

			WindowDropEvent event(std::move(filepaths));
			data.m_EventCallback(event);
		});

	WHP_CORE_INFO("[Application] Created Window {0} ({1}, {2})", props.m_Title, props.m_Width, props.m_Height);
}

void WindowsWindow::Shutdown()
{
	WHP_PROFILE_FUNCTION();

	if (!m_Window)
		return;

	glfwDestroyWindow(m_Window);
	m_Window = nullptr;

	if (s_GLFWWindowCount > 0)
		--s_GLFWWindowCount;

	if (s_GLFWWindowCount == 0)
		glfwTerminate();
}

void WindowsWindow::OnUpdate()
{
	WHP_PROFILE_FUNCTION();
	s_ScrollDelta = 0;
	glfwPollEvents();
	m_Context->SwapBuffers();
}

WHP_NODISCARD float WindowsWindow::GetScrollDelta() const
{
	return s_ScrollDelta;
}

std::pair<int, int> WindowsWindow::GetPosition() const
{
	int x, y;
	glfwGetWindowPos(m_Window, &x, &y);
	return { x, y };
}

void WindowsWindow::SetVsync(bool enabled)
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
	m_Data.m_Vsync = enabled;
}

bool WindowsWindow::IsVsync() const
{
	return m_Data.m_Vsync;
}

_WHIP_END
