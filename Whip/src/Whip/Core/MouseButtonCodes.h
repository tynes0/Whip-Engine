#pragma once

#include "Core.h"


_WHIP_START

using mouse_code = uint16_t;

namespace mouse
{
	enum : mouse_code
	{
		// From glfw3.h
		button0 = 0,
		button1 = 1,
		button2 = 2,
		button3 = 3,
		button4 = 4,
		button5 = 5,
		button6 = 6,
		button7 = 7,

		button_last = button7,
		button_left = button0,
		button_right = button1,
		button_middle = button2
	};

	inline const char* to_string(mouse_code code)
	{
		switch (code)
		{
		case button0: return "button0";
		case button1: return "button1";
		case button2: return "button2";
		case button3: return "button3";
		case button4: return "button4";
		case button5: return "button5";
		case button6: return "button6";
		case button7: return "button7";

		default: return "unknown";
		}
	}
}

_WHIP_END
