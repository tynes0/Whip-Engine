#pragma once

#include <Whip/Window.h>
#include <Whip/Render/GraphicsContext.h>

#include <GLFW/glfw3.h>


_WHIP_START

class WindowsWindow : public Window
{
private:
	GLFWwindow* m_Window;
	GraphicsContext* m_Context;
	WindowData m_Data;
private:
	virtual void Init(const WindowProps& props);
	virtual void Shutdown();
public:
	WindowsWindow(const WindowProps& props);
	virtual ~WindowsWindow();

	void OnUpdate() override;

	WHP_NODISCARD inline unsigned int GetWidth() const override { return m_Data.WinProps.Width; }
	WHP_NODISCARD inline unsigned int GetHeight() const override { return m_Data.WinProps.Height; }

	WHP_NODISCARD inline virtual void* GetNativeWindow() const override { return m_Window; }

	// Window attributes
	inline void SetEventCallback(const EventCallbackFn& callback) override { m_Data.EventCallback = callback; }
	void SetVSync(bool enabled) override;
	WHP_NODISCARD bool IsVSync() const override;
};

#define RESIZE_WIN_PROPS(win_props, width, height) win_props.Width = width, win_props.Height = height
#define SET_WIN_PROPS(win_props, title, width, height) RESIZE_WIN_PROPS(win_props, width, height), win_props.Title = title

_WHIP_END