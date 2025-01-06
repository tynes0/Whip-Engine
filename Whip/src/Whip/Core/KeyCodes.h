#pragma once

#include <Whip/Core/Core.h>

_WHIP_START

using key_code = uint16_t;

namespace key
{
	enum : key_code
	{
		// From glfw3.h
		space = 32,
		apostrophe = 39, /* ' */
		comma = 44, /* , */
		minus = 45, /* - */
		period = 46, /* . */
		slash = 47, /* / */

		D0 = 48, /* 0 */
		D1 = 49, /* 1 */
		D2 = 50, /* 2 */
		D3 = 51, /* 3 */
		D4 = 52, /* 4 */
		D5 = 53, /* 5 */
		D6 = 54, /* 6 */
		D7 = 55, /* 7 */
		D8 = 56, /* 8 */
		D9 = 57, /* 9 */

		semicolon = 59, /* ; */
		equal = 61, /* = */

		A = 65,
		B = 66,
		C = 67,
		D = 68,
		E = 69,
		F = 70,
		G = 71,
		H = 72,
		I = 73,
		J = 74,
		K = 75,
		L = 76,
		M = 77,
		N = 78,
		O = 79,
		P = 80,
		Q = 81,
		R = 82,
		S = 83,
		T = 84,
		U = 85,
		V = 86,
		W = 87,
		X = 88,
		Y = 89,
		Z = 90,

		left_bracket = 91,  /* [ */
		backslash = 92,  /* \ */
		right_bracket = 93,  /* ] */
		grave_accent = 96,  /* ` */

		world1 = 161, /* non-US #1 */
		world2 = 162, /* non-US #2 */

		/* Function keys */
		escape = 256,
		enter = 257,
		tab = 258,
		backspace = 259,
		insert = 260,
		del = 261,
		right = 262,
		left = 263,
		down = 264,
		up = 265,
		page_up = 266,
		page_down = 267,
		home = 268,
		end = 269,
		caps_lock = 280,
		scroll_lock = 281,
		num_lock = 282,
		print_screen = 283,
		pause = 284,
		F1 = 290,
		F2 = 291,
		F3 = 292,
		F4 = 293,
		F5 = 294,
		F6 = 295,
		F7 = 296,
		F8 = 297,
		F9 = 298,
		F10 = 299,
		F11 = 300,
		F12 = 301,
		F13 = 302,
		F14 = 303,
		F15 = 304,
		F16 = 305,
		F17 = 306,
		F18 = 307,
		F19 = 308,
		F20 = 309,
		F21 = 310,
		F22 = 311,
		F23 = 312,
		F24 = 313,
		F25 = 314,

		/* Keypad */
		KP0 = 320,
		KP1 = 321,
		KP2 = 322,
		KP3 = 323,
		KP4 = 324,
		KP5 = 325,
		KP6 = 326,
		KP7 = 327,
		KP8 = 328,
		KP9 = 329,
		KPdecimal = 330,
		KPdivide = 331,
		KPmultiply = 332,
		KPsubtract = 333,
		KPadd = 334,
		KPenter = 335,
		KPequal = 336,

		left_shift = 340,
		left_control = 341,
		left_alt = 342,
		left_super = 343,
		right_shift = 344,
		right_control = 345,
		right_alt = 346,
		right_super = 347,
		menu = 348
	};

	inline const char* to_string(key_code code)
	{
		switch (code)
		{
		case space: return "space";
		case apostrophe: return "apostrophe";
		case comma: return "comma";
		case minus: return "minus";
		case period: return "period";
		case slash: return "slash";

		case D0: return "D0";
		case D1: return "D1";
		case D2: return "D2";
		case D3: return "D3";
		case D4: return "D4";
		case D5: return "D5";
		case D6: return "D6";
		case D7: return "D7";
		case D8: return "D8";
		case D9: return "D9";

		case semicolon: return "semicolon";
		case equal: return "equal";

		case A: return "A";
		case B: return "B";
		case C: return "C";
		case D: return "D";
		case E: return "E";
		case F: return "F";
		case G: return "G";
		case H: return "H";
		case I: return "I";
		case J: return "J";
		case K: return "K";
		case L: return "L";
		case M: return "M";
		case N: return "N";
		case O: return "O";
		case P: return "P";
		case Q: return "Q";
		case R: return "R";
		case S: return "S";
		case T: return "T";
		case U: return "U";
		case V: return "V";
		case W: return "W";
		case X: return "X";
		case Y: return "Y";
		case Z: return "Z";

		case left_bracket: return "left_bracket";
		case backslash: return "backslash";
		case right_bracket: return "right_bracket";
		case grave_accent: return "grave_accent";

		case world1: return "world1";
		case world2: return "world2";

		case escape: return "escape";
		case enter: return "enter";
		case tab: return "tab";
		case backspace: return "backspace";
		case insert: return "insert";
		case del: return "delete";
		case right: return "right";
		case left: return "left";
		case down: return "down";
		case up: return "up";
		case page_up: return "page_up";
		case page_down: return "page_down";
		case home: return "home";
		case end: return "end";
		case caps_lock: return "caps_lock";
		case scroll_lock: return "scroll_lock";
		case num_lock: return "num_lock";
		case print_screen: return "print_screen";
		case pause: return "pause";
		case F1: return "F1";
		case F2: return "F2";
		case F3: return "F3";
		case F4: return "F4";
		case F5: return "F5";
		case F6: return "F6";
		case F7: return "F7";
		case F8: return "F8";
		case F9: return "F9";
		case F10: return "F10";
		case F11: return "F11";
		case F12: return "F12";
		case F13: return "F13";
		case F14: return "F14";
		case F15: return "F15";
		case F16: return "F16";
		case F17: return "F17";
		case F18: return "F18";
		case F19: return "F19";
		case F20: return "F20";
		case F21: return "F21";
		case F22: return "F22";
		case F23: return "F23";
		case F24: return "F24";
		case F25: return "F25";

		case KP0: return "KP0";
		case KP1: return "KP1";
		case KP2: return "KP2";
		case KP3: return "KP3";
		case KP4: return "KP4";
		case KP5: return "KP5";
		case KP6: return "KP6";
		case KP7: return "KP7";
		case KP8: return "KP8";
		case KP9: return "KP9";
		case KPdecimal: return "KPdecimal";
		case KPdivide: return "KPdivide";
		case KPmultiply: return "KPmultiply";
		case KPsubtract: return "KPsubtract";
		case KPadd: return "KPadd";
		case KPenter: return "KPenter";
		case KPequal: return "KPequal";

		case left_shift: return "left_shift";
		case left_control: return "left_control";
		case left_alt: return "left_alt";
		case left_super: return "left_super";
		case right_shift: return "right_shift";
		case right_control: return "right_control";
		case right_alt: return "right_alt";
		case right_super: return "right_super";
		case menu: return "menu";

		default: return "unknown";
		}
	}
}


_WHIP_END
