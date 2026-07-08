#pragma once

#include <Whip/Core/Window.h>
#include <Whip/Render/GraphicsContext.h>

#include <GLFW/glfw3.h>

#include <array>

_WHIP_START

class WindowsWindow : public Window  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	WindowsWindow(const WindowProps& props);
	~WindowsWindow() override;

	void OnUpdate() override;

	WHP_NODISCARD inline unsigned int GetWidth() const override { return m_Data.m_Properties.m_Width; }
	WHP_NODISCARD inline unsigned int GetHeight() const override { return m_Data.m_Properties.m_Height; }
	WHP_NODISCARD virtual float GetScrollDelta() const override;
	WHP_NODISCARD float GetScrollDeltaX() const override;
	WHP_NODISCARD float GetScrollDeltaY() const override;

	WHP_NODISCARD std::pair<int, int> GetPosition() const override;
	void SetPosition(int x, int y) override;
	void SetSize(uint32_t width, uint32_t height) override;
	void Minimize() override;
	void Maximize() override;
	void Restore() override;
	WHP_NODISCARD bool IsMaximized() const override;

	WHP_NODISCARD void* GetNativeWindow() const override;

	// Window attributes
	void SetEventCallback(const EventCallbackFn& callback) override;
	void SetVsync(bool enabled) override;
	WHP_NODISCARD bool IsVsync() const override;
	void SetCursorMode(CursorMode mode) override;
	WHP_NODISCARD CursorMode GetCursorMode() const override;
	void SetCursorShape(CursorShape shape) override;
	WHP_NODISCARD CursorShape GetCursorShape() const override;
private:
	virtual void Init(const WindowProps& props);
	virtual void Shutdown();
	void CreateStandardCursors();
	void DestroyStandardCursors();
private:
	GLFWwindow* m_Window;
	GraphicContext* m_Context;
	WindowData m_Data;
	std::array<GLFWcursor*, static_cast<size_t>(CursorShape::Count)> m_StandardCursors{};
	CursorMode m_CursorMode = CursorMode::Normal;
	CursorShape m_CursorShape = CursorShape::Arrow;
};

_WHIP_END
