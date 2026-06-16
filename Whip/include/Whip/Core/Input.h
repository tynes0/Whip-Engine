#pragma once

#include "KeyCodes.h"
#include "MouseButtonCodes.h"
#include "memory.h"

_WHIP_START

class Input
{
public:
	WHP_NODISCARD static bool IsKeyPressed(int keyCode);
	WHP_NODISCARD static bool IsKeyReleased(int keyCode);
	WHP_NODISCARD static bool IsKeyDown(int keyCode);
	WHP_NODISCARD static bool IsKeyUp(int keyCode);
	WHP_NODISCARD static bool IsMouseButtonPressed(int button);
	WHP_NODISCARD static bool IsMouseButtonReleased(int button);
	WHP_NODISCARD static bool IsMouseButtonUp(int button);
	WHP_NODISCARD static bool IsMouseButtonDown(int button);
	WHP_NODISCARD static float GetMouseX();
	WHP_NODISCARD static float GetMouseY();
	WHP_NODISCARD static std::pair<float, float> GetMousePosition();
	WHP_NODISCARD static float GetScrollDelta();
};

_WHIP_END
