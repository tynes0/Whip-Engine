#pragma once

#include <Whip/Render/GraphicsContext.h>

struct GLFWwindow;

_WHIP_START

class OpenGLContext : public GraphicContext
{
private:
	GLFWwindow* m_WindowHandle;
public:
	OpenGLContext(GLFWwindow* windowHandle);
	virtual void Init() override;
	virtual void SwapBuffers() override;
};

_WHIP_END
