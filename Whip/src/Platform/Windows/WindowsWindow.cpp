#include <WhipPch.h>
#include "WindowsWindow.h"

#include <Platform/OpenGL/OpenGLContext.h>

#include <Whip/Events/ApplicationEvent.h>
#include <Whip/Events/KeyEvent.h>
#include <Whip/Events/MouseEvent.h>

#include <Whip/Debug/Instrumentor.h>

#ifdef WHP_PLATFORM_WINDOWS
	#define GLFW_EXPOSE_NATIVE_WIN32
	#include <GLFW/glfw3native.h>
	#include <Windows.h>
	#include <windowsx.h>
	#ifdef IsMaximized
		#undef IsMaximized
	#endif
#endif

_WHIP_START

namespace
{
	uint32_t s_GLFWWindowCount = 0;
	float s_ScrollDelta = 0.0f;

#ifdef WHP_PLATFORM_WINDOWS
	constexpr int CustomTitlebarHitTestHeight = 30;
	constexpr int CustomTitlebarControlWidth = 46 * 3;
	constexpr int CustomResizeBorder = 8;

	memory::UnorderedMap<HWND, WNDPROC> s_PreviousWindowProcedures;

	LRESULT CALLBACK CustomTitlebarWindowProc(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam)
	{
		auto procedureIterator = s_PreviousWindowProcedures.find(windowHandle);
		WNDPROC previousProcedure = procedureIterator != s_PreviousWindowProcedures.end() ? procedureIterator->second : nullptr;

		if (message == WM_NCCALCSIZE && wParam == TRUE)
		{
			if (IsZoomed(windowHandle) != FALSE)
			{
				auto* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);
				MONITORINFO monitorInfo{};
				monitorInfo.cbSize = sizeof(MONITORINFO);
				if (GetMonitorInfoW(MonitorFromWindow(windowHandle, MONITOR_DEFAULTTONEAREST), &monitorInfo))
					params->rgrc[0] = monitorInfo.rcWork;
			}
			return 0;
		}

		if (message == WM_NCHITTEST)
		{
			RECT rect{};
			GetClientRect(windowHandle, &rect);

			POINT point{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
			ScreenToClient(windowHandle, &point);

			const bool maximized = IsZoomed(windowHandle) != FALSE;
			const bool left = point.x >= rect.left && point.x < rect.left + CustomResizeBorder;
			const bool right = point.x <= rect.right && point.x > rect.right - CustomResizeBorder;
			const bool top = point.y >= rect.top && point.y < rect.top + CustomResizeBorder;
			const bool bottom = point.y <= rect.bottom && point.y > rect.bottom - CustomResizeBorder;

			if (!maximized)
			{
				if (top && left) return HTTOPLEFT;
				if (top && right) return HTTOPRIGHT;
				if (bottom && left) return HTBOTTOMLEFT;
				if (bottom && right) return HTBOTTOMRIGHT;
				if (left) return HTLEFT;
				if (right) return HTRIGHT;
				if (top) return HTTOP;
				if (bottom) return HTBOTTOM;
			}

			const bool inTitlebar = point.y >= rect.top && point.y < rect.top + CustomTitlebarHitTestHeight;
			const bool inCaptionButtons = point.x >= rect.right - CustomTitlebarControlWidth && point.x <= rect.right;
			if (inTitlebar && !inCaptionButtons)
				return HTCAPTION;
		}

		if (previousProcedure)
			return CallWindowProcW(previousProcedure, windowHandle, message, wParam, lParam);
		return DefWindowProcW(windowHandle, message, wParam, lParam);
	}

	void InstallCustomTitlebarHook(GLFWwindow* window)
	{
		HWND windowHandle = glfwGetWin32Window(window);
		if (!windowHandle || s_PreviousWindowProcedures.contains(windowHandle))
			return;

		WNDPROC previousProcedure = reinterpret_cast<WNDPROC>(
			SetWindowLongPtrW(windowHandle, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(CustomTitlebarWindowProc)));
		s_PreviousWindowProcedures[windowHandle] = previousProcedure;

		const LONG_PTR style = GetWindowLongPtrW(windowHandle, GWL_STYLE);
		SetWindowLongPtrW(windowHandle, GWL_STYLE, style | WS_CAPTION | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
		SetWindowPos(windowHandle, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
	}

	void UninstallCustomTitlebarHook(GLFWwindow* window)
	{
		if (!window)
			return;

		HWND windowHandle = glfwGetWin32Window(window);
		auto procedureIterator = s_PreviousWindowProcedures.find(windowHandle);
		if (procedureIterator == s_PreviousWindowProcedures.end())
			return;

		SetWindowLongPtrW(windowHandle, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(procedureIterator->second));
		s_PreviousWindowProcedures.erase(procedureIterator);
	}
#endif
}


#ifdef WHP_PLATFORM_WINDOWS

WHP_NODISCARD Window* Window::Create(const WindowProps& props)
{
	return MakeRawTagged<WindowsWindow>(memory::MemoryTag::Core, props);
}

#endif // WHP_PLATFORM_WINDOWS

WindowsWindow::WindowsWindow(const WindowProps& props)
{
	WHP_PROFILE_FUNCTION();

	WindowsWindow::Init(props);
}

WindowsWindow::~WindowsWindow()
{
	WHP_PROFILE_FUNCTION();

	WindowsWindow::Shutdown();
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

	glfwWindowHint(GLFW_MAXIMIZED, props.m_Fullscreen);
	glfwWindowHint(GLFW_DECORATED, props.m_CustomTitlebar ? GLFW_FALSE : GLFW_TRUE);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	// create Window
	{
		WHP_PROFILE_SCOPE("glfwCreateWindow");
		m_Data.m_Properties = props;
		m_Window = glfwCreateWindow(static_cast<int>(props.m_Width), static_cast<int>(props.m_Height), m_Data.m_Properties.m_Title.c_str(), nullptr, nullptr);
		++s_GLFWWindowCount;
	}

	if (props.m_Fullscreen)
	{
		int width = 0, height = 0;
		glfwGetWindowSize(m_Window, &width, &height);
		m_Data.m_Properties.m_Width = width;
		m_Data.m_Properties.m_Height = height;
	}

#ifdef WHP_PLATFORM_WINDOWS
	if (props.m_CustomTitlebar)
		InstallCustomTitlebarHook(m_Window);
#endif

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
				default: break;
			}
		});

	glfwSetCharCallback(m_Window, [](GLFWwindow* window, unsigned int keyCode)->void
	{
		WindowData& data = DREF(WindowData*)glfwGetWindowUserPointer(window);
		KeyTypedEvent event{ static_cast<KeyCode>(keyCode) };
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
				default: break;
			}
		});
	glfwSetScrollCallback(m_Window, [](GLFWwindow* window, double offsetX, double offsetY)->void
		{
			WindowData& data = DREF(WindowData*)glfwGetWindowUserPointer(window);
			MouseScrolledEvent event(static_cast<float>(offsetX), static_cast<float>(offsetY));
			data.m_EventCallback(event);
			s_ScrollDelta = static_cast<float>(offsetY);
		});
	glfwSetCursorPosCallback(m_Window, [](GLFWwindow* window, double posX, double posY)->void
		{
			WindowData& data = DREF(WindowData*)glfwGetWindowUserPointer(window);
			MouseMovedEvent event(static_cast<float>(posX), static_cast<float>(posY));
			data.m_EventCallback(event);
		});

	glfwSetDropCallback(m_Window, [](GLFWwindow* window, int pathCount, const char* paths[])
		{
			WindowData& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));

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

#ifdef WHP_PLATFORM_WINDOWS
	if (m_Data.m_Properties.m_CustomTitlebar)
		UninstallCustomTitlebarHook(m_Window);
#endif

	DeleteRaw(m_Context);
	m_Context = nullptr;

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

void WindowsWindow::SetPosition(int x, int y)
{
	glfwSetWindowPos(m_Window, x, y);
}

void WindowsWindow::SetSize(uint32_t width, uint32_t height)
{
	m_Data.m_Properties.m_Width = width;
	m_Data.m_Properties.m_Height = height;
	glfwSetWindowSize(m_Window, static_cast<int>(width), static_cast<int>(height));
}

void WindowsWindow::Minimize()
{
	glfwIconifyWindow(m_Window);
}

void WindowsWindow::Maximize()
{
	glfwMaximizeWindow(m_Window);
}

void WindowsWindow::Restore()
{
	glfwRestoreWindow(m_Window);
	int width = 0, height = 0;
	glfwGetWindowSize(m_Window, &width, &height);
	m_Data.m_Properties.m_Width = static_cast<uint32_t>(std::max(width, 0));
	m_Data.m_Properties.m_Height = static_cast<uint32_t>(std::max(height, 0));
}

bool WindowsWindow::IsMaximized() const
{
	return glfwGetWindowAttrib(m_Window, GLFW_MAXIMIZED) == GLFW_TRUE;
}

void* WindowsWindow::GetNativeWindow() const
{
	return m_Window;
}

void WindowsWindow::SetEventCallback(const EventCallbackFn& callback)
{
	m_Data.m_EventCallback = callback;
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
