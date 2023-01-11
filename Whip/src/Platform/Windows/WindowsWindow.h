#pragma once

#include <Whip/Window.h>

#include <GLFW/glfw3.h>

_WHIP_START

struct WindowData
{
	WindowProps WinProps;
	bool VSync;

	Window::EventCallbackFn EventCallback;
};

class WindowsWindow : public Window
{
private:
	GLFWwindow* m_Window;

	WindowData m_Data;
private:
	virtual void Init(const WindowProps& props);
	virtual void Shutdown();
public:
	WindowsWindow(const WindowProps& props);
	virtual ~WindowsWindow();

	void OnUpdate() override;

	inline unsigned int GetWidth() const override { return m_Data.WinProps.Width; }
	inline unsigned int GetHeight() const override { return m_Data.WinProps.Height; }

	// Window attributes
	inline void SetEventCallback(const EventCallbackFn& callback) override { m_Data.EventCallback = callback; }
	void SetVSync(bool enabled) override;
	bool IsVSync() const override;
};

#define SET_WIN_PROPS(windowProps, title, width, height) windowProps.Title = title, windowProps.Width = width, windowProps.Height = height

_WHIP_END