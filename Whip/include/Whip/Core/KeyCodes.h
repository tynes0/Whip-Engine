#pragma once

#include <Whip/Core/Core.h>

_WHIP_START

using KeyCode = uint16_t;

namespace Key
{
	enum : KeyCode
	{
		// From glfw3.h
		
		Space = 32,
		Apostrophe = 39, /* ' */
		Comma = 44, /* , */
		Minus = 45, /* - */
		Period = 46, /* . */
		Slash = 47, /* / */

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

		Semicolon = 59, /* ; */
		Equal = 61, /* = */

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

		LeftBracket = 91,  /* [ */
		Backslash = 92,  /* \ */
		RightBracket = 93,  /* ] */
		GraveAccent = 96,  /* ` */

		World1 = 161, /* non-US #1 */
		World2 = 162, /* non-US #2 */

		/* Function keys */
		Escape = 256,
		Enter = 257,
		Tab = 258,
		Backspace = 259,
		Insert = 260,
		Delete = 261,
		Right = 262,
		Left = 263,
		Down = 264,
		Up = 265,
		PageUp = 266,
		PageDown = 267,
		Home = 268,
		End = 269,
		CapsLock = 280,
		ScrollLock = 281,
		NumLock = 282,
		PrintScreen = 283,
		Pause = 284,
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
		KPDecimal = 330,
		KPDivide = 331,
		KPMultiply = 332,
		KPSubtract = 333,
		KPAdd = 334,
		KPEnter = 335,
		KPEqual = 336,

		LeftShift = 340,
		LeftControl = 341,
		LeftAlt = 342,
		LeftSuper = 343,
		RightShift = 344,
		RightControl = 345,
		RightAlt = 346,
		RightSuper = 347,
		Menu = 348
	};

	inline const char* ToString(KeyCode code)
	{
		switch (code)
		{
		case Space: return "Space";
		case Apostrophe: return "Apostrophe";
		case Comma: return "Comma";
		case Minus: return "Minus";
		case Period: return "Period";
		case Slash: return "Slash";

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

		case Semicolon: return "Semicolon";
		case Equal: return "Equal";

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

		case LeftBracket: return "LeftBracket";
		case Backslash: return "Backslash";
		case RightBracket: return "RightBracket";
		case GraveAccent: return "GraveAccent";

		case World1: return "World1";
		case World2: return "World2";

		case Escape: return "Escape";
		case Enter: return "Enter";
		case Tab: return "Tab";
		case Backspace: return "Backspace";
		case Insert: return "Insert";
		case Delete: return "Delete";
		case Right: return "Right";
		case Left: return "Left";
		case Down: return "Down";
		case Up: return "Up";
		case PageUp: return "PageUp";
		case PageDown: return "PageDown";
		case Home: return "Home";
		case End: return "End";
		case CapsLock: return "CapsLock";
		case ScrollLock: return "ScrollLock";
		case NumLock: return "NumLock";
		case PrintScreen: return "PrintScreen";
		case Pause: return "Pause";
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
		case KPDecimal: return "KPDecimal";
		case KPDivide: return "KPDivide";
		case KPMultiply: return "KPMultiply";
		case KPSubtract: return "KPSubtract";
		case KPAdd: return "KPAdd";
		case KPEnter: return "KPEnter";
		case KPEqual: return "KPEqual";

		case LeftShift: return "LeftShift";
		case LeftControl: return "LeftControl";
		case LeftAlt: return "LeftAlt";
		case LeftSuper: return "LeftSuper";
		case RightShift: return "RightShift";
		case RightControl: return "RightControl";
		case RightAlt: return "RightAlt";
		case RightSuper: return "RightSuper";
		case Menu: return "Menu";

		default: return "Unknown";
		}
	}
}


_WHIP_END
