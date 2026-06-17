#pragma once

#include <sstream>

#include <Whip/Events/Event.h>


_WHIP_START

// Window properties
struct WindowProps
{
	std::string m_Title = "Whip Application";
	uint32_t m_Width = 1280;
	uint32_t m_Height = 720;
	bool m_Fullscreen = false;
	bool m_CustomTitlebar = false;
};

// interface representing a desktop system  based Window
class Window
{
public:
	using EventCallbackFn = std::function<void(Event&)>;
	virtual ~Window() {}

	virtual void OnUpdate() = 0;

	WHP_NODISCARD virtual uint32_t GetWidth() const = 0;
	WHP_NODISCARD virtual uint32_t GetHeight() const = 0;
	WHP_NODISCARD virtual float GetScrollDelta() const = 0;

	WHP_NODISCARD virtual std::pair<int, int> GetPosition() const = 0;
	virtual void SetPosition(int x, int y) = 0;
	virtual void SetSize(uint32_t width, uint32_t height) = 0;
	virtual void Minimize() = 0;
	virtual void Maximize() = 0;
	virtual void Restore() = 0;
	WHP_NODISCARD virtual bool IsMaximized() const = 0;

	// Window attributes
	virtual void SetEventCallback(const EventCallbackFn& callback) = 0;
	virtual void SetVsync(bool enabled) = 0;
	WHP_NODISCARD virtual bool IsVsync() const = 0;

	WHP_NODISCARD virtual void* GetNativeWindow() const = 0;

	WHP_NODISCARD static Window* Create(const WindowProps& props = WindowProps());
};

struct WindowData
{
	WindowProps m_Properties;
	bool m_Vsync = false;

	Window::EventCallbackFn m_EventCallback;
};

_WHIP_END
