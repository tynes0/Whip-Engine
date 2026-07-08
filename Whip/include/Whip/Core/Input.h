#pragma once

#include "CursorCodes.h"
#include "KeyCodes.h"
#include "MouseButtonCodes.h"
#include "memory.h"

#include <glm/glm.hpp>

_WHIP_START

class Input
{
public:
	static void BeginFrame();

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
	WHP_NODISCARD static glm::vec2 GetMouseDelta();
	WHP_NODISCARD static float GetMouseDeltaX();
	WHP_NODISCARD static float GetMouseDeltaY();
	WHP_NODISCARD static glm::vec2 GetMouseViewportPosition();
	WHP_NODISCARD static bool IsMouseInsideViewport();
	WHP_NODISCARD static float GetScrollDelta();
	WHP_NODISCARD static float GetScrollDeltaX();
	WHP_NODISCARD static float GetScrollDeltaY();

	static void SetViewportState(bool hovered, bool focused, const glm::vec2& min, const glm::vec2& max);
	WHP_NODISCARD static bool IsViewportHovered();
	WHP_NODISCARD static bool IsViewportFocused();
	static void SetRuntimeInputEnabled(bool enabled);
	WHP_NODISCARD static bool IsRuntimeInputEnabled();
	WHP_NODISCARD static bool IsRuntimeInputActive();

	static void SetCursorMode(CursorMode mode);
	WHP_NODISCARD static CursorMode GetCursorMode();
	static void SetCursorVisible(bool visible);
	WHP_NODISCARD static bool IsCursorVisible();
	static void SetCursorShape(CursorShape shape);
	WHP_NODISCARD static CursorShape GetCursorShape();
};

_WHIP_END
