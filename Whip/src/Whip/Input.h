#pragma once

#include <Whip/KeyCodes.h>

_WHIP_START

class WHIP_API Input
{
private:
	static Input* s_Instance;
protected:
	WHP_NODISCARD virtual bool isKeyPressedImpl(int keycode) = 0;
	WHP_NODISCARD virtual bool isMouseButtonPressedImpl(int button) = 0;
	WHP_NODISCARD virtual float getMousePosXImpl() = 0;
	WHP_NODISCARD virtual float getMousePosYImpl() = 0;
	WHP_NODISCARD virtual pair<float, float> getMousePosImpl() = 0;
public:
	WHP_NODISCARD inline static bool isKeyPressed(int keycode) { return s_Instance->isKeyPressedImpl(keycode); }
	WHP_NODISCARD inline static bool isMouseButtonPressed(int button) { return s_Instance->isMouseButtonPressedImpl(button); }
	WHP_NODISCARD inline static float getMousePosX() { return s_Instance->getMousePosXImpl(); }
	WHP_NODISCARD inline static float getMousePosY() { return s_Instance->getMousePosYImpl(); }
	WHP_NODISCARD inline static pair<float, float> getMousePos() { return s_Instance->getMousePosImpl(); }

};

_WHIP_END