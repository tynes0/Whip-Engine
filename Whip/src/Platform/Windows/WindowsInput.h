#pragma once

#include <Whip/Input.h>

_WHIP_START

class WindowsInput : public Input
{
protected:
	WHP_NODISCARD virtual bool isKeyPressedImpl(int keycode) override;
	WHP_NODISCARD virtual bool isMouseButtonPressedImpl(int button) override;
	WHP_NODISCARD virtual float getMousePosXImpl() override;
	WHP_NODISCARD virtual float getMousePosYImpl() override;
	WHP_NODISCARD virtual std::pair<float, float> getMousePosImpl() override;
};

_WHIP_END