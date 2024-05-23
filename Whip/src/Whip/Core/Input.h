#pragma once

#include <Whip/Core/KeyCodes.h>
#include <utility>

_WHIP_START

class input
{
private:
	static input* s_instance;
protected:
	WHP_NODISCARD virtual bool is_key_pressed_impl(int keycode) = 0;
	WHP_NODISCARD virtual bool is_key_released_impl(int keycode) = 0;
	WHP_NODISCARD virtual bool is_mouse_button_pressed_impl(int button) = 0;
	WHP_NODISCARD virtual float get_mouse_posx_impl() = 0;
	WHP_NODISCARD virtual float get_mouse_posy_impl() = 0;
	WHP_NODISCARD virtual std::pair<float, float> get_mouse_pos_impl() = 0;
public:
	WHP_NODISCARD inline static bool is_key_pressed(int keycode) { return s_instance->is_key_pressed_impl(keycode); }
	WHP_NODISCARD inline static bool is_key_released(int keycode) { return s_instance->is_key_released_impl(keycode); }
	WHP_NODISCARD inline static bool is_mouse_button_pressed(int button) { return s_instance->is_mouse_button_pressed_impl(button); }
	WHP_NODISCARD inline static float get_mouse_positionx() { return s_instance->get_mouse_posx_impl(); }
	WHP_NODISCARD inline static float get_mouse_positiony() { return s_instance->get_mouse_posy_impl(); }
	WHP_NODISCARD inline static std::pair<float, float> get_mouse_position() { return s_instance->get_mouse_pos_impl(); }

};

_WHIP_END