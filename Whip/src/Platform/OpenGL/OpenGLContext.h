#pragma once

#include <Whip/Render/GraphicsContext.h>

struct GLFWwindow;

_WHIP_START

class opengl_context : public graphic_context
{
private:
	GLFWwindow* m_window_handle;
public:
	opengl_context(GLFWwindow* win_handle);
	virtual void init() override;
	virtual void swap_buffers() override;
};

_WHIP_END