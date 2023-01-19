#pragma once

#include <Whip/Render/GraphicsContext.h>

struct GLFWwindow;

_WHIP_START

class OpenGLContext : public GraphicsContext
{
private:
	GLFWwindow* m_WindowHandle;
public:
	OpenGLContext(GLFWwindow* WinHandle);
	virtual void Init() override;
	virtual void SwapBuffers() override;
};

_WHIP_END