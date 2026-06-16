#pragma once

#include <Whip/Core/Core.h>
#include "GLFW/glfw3.h"

_WHIP_START

class GraphicContext
{
public:
	virtual ~GraphicContext() = default;

	virtual void Init() = 0;
	virtual void SwapBuffers() = 0;

	WHP_NODISCARD static GraphicContext* Create(GLFWwindow* windowHandle);
};

_WHIP_END
