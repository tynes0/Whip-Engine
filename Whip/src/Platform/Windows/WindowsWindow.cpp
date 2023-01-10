#include "whippch.h"
#include "WindowsWindow.h"

#include "Whip/Events/ApplicationEvent.h"
#include "Whip/Events/KeyEvent.h"
#include "Whip/Events/MouseEvent.h"

_WHIP_START

static bool s_GLFWInitialized = false;

static void GLFWErrorCallback(int err, const char* desc)
{
	WHP_CORE_ERROR("GLFW Error ({0}): {1}", err, desc);
}


Window* Window::Create(const WindowProps& props)
{
	return new WindowsWindow(props);
}

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
	m_Data.Title = props.Title;
	m_Data.Width = props.Width;
	m_Data.Height = props.Height;

	WHP_CORE_INFO("Creating Window {0} ({1}, {2})", props.Title, props.Width, props.Height);

	if (!s_GLFWInitialized)
	{
		int success = glfwInit();
		WHP_CORE_ASSERT(success, "Could not initialize GLFW!");
		glfwSetErrorCallback(GLFWErrorCallback);
		s_GLFWInitialized = true;
	}

	m_Window = glfwCreateWindow((int)props.Width, (int)props.Height, m_Data.Title.c_str(), nullptr, nullptr);
	glfwMakeContextCurrent(m_Window);
	glfwSetWindowUserPointer(m_Window, &m_Data);
	SetVSync(true);

	// Set GLFW callbacks
	glfwSetWindowSizeCallback(m_Window, [](GLFWwindow* window, int width, int height)
		{
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
			data.Width = width;
			data.Height = height;
			WindowResizeEvent event(width, height);
			data.EventCallback(event);
		});
	glfwSetWindowCloseCallback(m_Window, [](GLFWwindow* window)
		{
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
			WindowCloseEvent event;
			data.EventCallback(event);
		});

	glfwSetKeyCallback(m_Window, [](GLFWwindow* window, int key, int scanmode, int action, int mods)
		{
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
			switch (action)
			{
				case GLFW_RELEASE:
				{
					KeyReleasedEvent event(key);
					data.EventCallback(event);
					break;
				}
				case GLFW_PRESS:
				{
					KeyPressedEvent event(key, 0);
					data.EventCallback(event);
					break;
				}

				case GLFW_REPEAT:
				{
					// will change in future
					KeyPressedEvent event(key, 1);
					data.EventCallback(event);
					break;
				}
			}
		});
	glfwSetMouseButtonCallback(m_Window, [](GLFWwindow* window, int button, int action, int mods) 
		{
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
			switch (action)
			{
				case GLFW_RELEASE:
				{
					MouseButtonReleasedEvent event(button);
					data.EventCallback(event);
					break;
				}
				case GLFW_PRESS:
				{
					MouseButtonPressedEvent event(button);
					data.EventCallback(event);
					break;
				}
			}
		});
	glfwSetScrollCallback(m_Window, [](GLFWwindow* window, double OffsetX, double OffsetY)
		{
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
			MouseScrolledEvent event((float)OffsetX, (float)OffsetY);
			data.EventCallback(event);
		});
	glfwSetCursorPosCallback(m_Window, [](GLFWwindow* window, double posX, double posY) 
		{
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
			MouseMovedEvent event((float)posX, (float)posY);
			data.EventCallback(event);
		});
}

void WindowsWindow::Shutdown()
{
	glfwDestroyWindow(m_Window);
}

void WindowsWindow::OnUpdate()
{
	glfwPollEvents();
	glfwSwapBuffers(m_Window);
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

bool WindowsWindow::IsVSync() const
{
	return m_Data.VSync;
}

_WHIP_END