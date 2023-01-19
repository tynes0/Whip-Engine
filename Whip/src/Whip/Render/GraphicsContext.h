#pragma once

#include <Whip/Core.h>

_WHIP_START

class GraphicsContext
{
public:
	virtual void Init() = 0;
	virtual void SwapBuffers() = 0;
};

_WHIP_END