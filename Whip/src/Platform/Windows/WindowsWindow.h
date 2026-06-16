#pragma once

#include <Whip/Core/Window.h>
#include <Whip/Render/GraphicsContext.h>

#include <GLFW/glfw3.h>


_WHIP_START

class WindowsWindow : public Window
{
public:
	WindowsWindow(const WindowProps& props);
	virtual ~WindowsWindow();

	void OnUpdate() override;

	WHP_NODISCARD inline unsigned int GetWidth() const override { return m_Data.m_Properties.m_Width; }
	WHP_NODISCARD inline unsigned int GetHeight() const override { return m_Data.m_Properties.m_Height; }
	WHP_NODISCARD virtual float GetScrollDelta() const override;

	WHP_NODISCARD std::pair<int, int> GetPosition() const override;

	WHP_NODISCARD inline virtual void* GetNativeWindow() const override { return m_Window; }

	// Window attributes
	inline void SetEventCallback(const EventCallbackFn& callback) override { m_Data.m_EventCallback = callback; }
	void SetVsync(bool enabled) override;
	WHP_NODISCARD bool IsVsync() const override;
private:
	virtual void Init(const WindowProps& props);
	virtual void Shutdown();
private:
	GLFWwindow* m_Window;
	GraphicContext* m_Context;
	WindowData m_Data;
};

_WHIP_END
