#pragma once

#include <Whip/Core/KeyCodes.h>
#include <Whip/Core/Utility.h>

_WHIP_START

class input
{
private:
	static input* s_instance;
protected:
	WHP_NODISCARD virtual bool is_key_pressed_impl(int keycode) = 0;
	WHP_NODISCARD virtual bool is_mouse_button_pressed_impl(int button) = 0;
	WHP_NODISCARD virtual float get_mouse_posx_impl() = 0;
	WHP_NODISCARD virtual float get_mouse_posy_impl() = 0;
	WHP_NODISCARD virtual pair<float, float> get_mouse_pos_impl() = 0;
public:
	WHP_NODISCARD inline static bool is_key_pressed(int keycode) { return s_instance->is_key_pressed_impl(keycode); }
	WHP_NODISCARD inline static bool is_mouse_button_pressed(int button) { return s_instance->is_mouse_button_pressed_impl(button); }
	WHP_NODISCARD inline static float get_mouse_posx() { return s_instance->get_mouse_posx_impl(); }
	WHP_NODISCARD inline static float get_mouse_posy() { return s_instance->get_mouse_posy_impl(); }
	WHP_NODISCARD inline static pair<float, float> get_mouse_pos() { return s_instance->get_mouse_pos_impl(); }

};

_WHIP_END