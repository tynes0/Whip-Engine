#pragma once

#include "KeyCodes.h"
#include "MouseButtonCodes.h"
#include "memory.h"

_WHIP_START

class input
{
public:
	WHP_NODISCARD static bool is_key_pressed(int keycode);
	WHP_NODISCARD static bool is_key_released(int keycode);
	WHP_NODISCARD static bool is_key_down(int keycode);
	WHP_NODISCARD static bool is_key_up(int keycode);
	WHP_NODISCARD static bool is_mouse_button_pressed(int button);
	WHP_NODISCARD static bool is_mouse_button_released(int button);
	WHP_NODISCARD static bool is_mouse_button_up(int button);
	WHP_NODISCARD static bool is_mouse_button_down(int button);
	WHP_NODISCARD static float get_mouse_X();
	WHP_NODISCARD static float get_mouse_Y();
	WHP_NODISCARD static std::pair<float, float> get_mouse_position();
	WHP_NODISCARD static float get_scroll_delta();
};

_WHIP_END
