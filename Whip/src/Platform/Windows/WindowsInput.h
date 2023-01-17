#pragma once

#include <Whip/Input.h>

_WHIP_START

class WindowsInput : public Input
{
protected:
	virtual bool isKeyPressedImpl(int keycode) override;
	virtual bool isMouseButtonPressedImpl(int button) override;
	virtual float getMousePosXImpl() override;
	virtual float getMousePosYImpl() override;
	virtual std::pair<float, float> getMousePosImpl() override;
};

_WHIP_END