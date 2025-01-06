#pragma once

#include <Whip/Core/Core.h>
#include "GLFW/glfw3.h"

_WHIP_START

class graphic_context
{
public:
	virtual ~graphic_context() = default;

	virtual void init() = 0;
	virtual void swap_buffers() = 0;

	WHP_NODISCARD static graphic_context* create(GLFWwindow* win_handle);
};

_WHIP_END