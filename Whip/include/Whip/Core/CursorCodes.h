#pragma once

#include <Whip/Core/Core.h>

_WHIP_START

enum class CursorMode : uint8_t
{
	Normal = 0,
	Hidden,
	Locked,
	Confined
};

enum class CursorShape : uint8_t
{
	Arrow = 0,
	IBeam,
	Crosshair,
	Hand,
	ResizeHorizontal,
	ResizeVertical,
	NotAllowed,
	Count
};

_WHIP_END
