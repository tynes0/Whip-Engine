#pragma once

#include <Whip/KeyCodes.h>

_WHIP_START

class WHIP_API Input
{
private:
	static Input* s_Instance;
protected:
	virtual bool isKeyPressedImpl(int keycode) = 0;
	virtual bool isMouseButtonPressedImpl(int button) = 0;
	virtual float getMousePosXImpl() = 0;
	virtual float getMousePosYImpl() = 0;
	virtual std::pair<float, float> getMousePosImpl() = 0;
public:
	inline static bool isKeyPressed(int keycode) { return s_Instance->isKeyPressedImpl(keycode); }
	inline static bool isMouseButtonPressed(int button) { return s_Instance->isMouseButtonPressedImpl(button); }
	inline static float getMousePosX() { return s_Instance->getMousePosXImpl(); }
	inline static float getMousePosY() { return s_Instance->getMousePosYImpl(); }
	inline static std::pair<float, float> getMousePos() { return s_Instance->getMousePosImpl(); }

};

_WHIP_END