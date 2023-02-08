#pragma once

#include <Whip/Core/Core.h>

_WHIP_START

class graphic_context
{
public:
	virtual void init() = 0;
	virtual void swap_buffers() = 0;
};

_WHIP_END