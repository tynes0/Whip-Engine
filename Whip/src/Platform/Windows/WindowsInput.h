#pragma once

#include <Whip/Core/Input.h>

_WHIP_START

class windows_input : public input
{
protected:
	WHP_NODISCARD virtual bool is_key_pressed_impl(int keycode) override;
	WHP_NODISCARD virtual bool is_key_released_impl(int keycode) override;
	WHP_NODISCARD virtual bool is_mouse_button_pressed_impl(int button) override;
	WHP_NODISCARD virtual float get_mouse_posx_impl() override;
	WHP_NODISCARD virtual float get_mouse_posy_impl() override;
	WHP_NODISCARD virtual std::pair<float, float> get_mouse_pos_impl() override;
};

_WHIP_END