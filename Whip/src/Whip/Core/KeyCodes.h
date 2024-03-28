#pragma once

#include <Whip/Core/Core.h>

#if !defined (WHP_KEYCODES)
#define WHP_KEYCODES


/* The unknown key */
#define WHP_KEY_UNKNOWN            -1
		
/* Printable keys */
#define WHP_KEY_SPACE              32 
#define WHP_KEY_APOSTROPHE         39  /* ' */
#define WHP_KEY_COMMA              44  /* , */
#define WHP_KEY_MINUS              45  /* - */
#define WHP_KEY_PERIOD             46  /* . */
#define WHP_KEY_SLASH              47  /* / */
#define WHP_KEY_0                  48
#define WHP_KEY_1                  49
#define WHP_KEY_2                  50
#define WHP_KEY_3                  51
#define WHP_KEY_4                  52
#define WHP_KEY_5                  53
#define WHP_KEY_6                  54
#define WHP_KEY_7                  55
#define WHP_KEY_8                  56
#define WHP_KEY_9                  57
#define WHP_KEY_SEMICOLON          59  /* ; */
#define WHP_KEY_EQUAL              61  /* = */
#define WHP_KEY_A                  65
#define WHP_KEY_B                  66
#define WHP_KEY_C                  67
#define WHP_KEY_D                  68
#define WHP_KEY_E                  69
#define WHP_KEY_F                  70
#define WHP_KEY_G                  71
#define WHP_KEY_H                  72
#define WHP_KEY_I                  73
#define WHP_KEY_J                  74
#define WHP_KEY_K                  75
#define WHP_KEY_L                  76
#define WHP_KEY_M                  77
#define WHP_KEY_N                  78
#define WHP_KEY_O                  79
#define WHP_KEY_P                  80
#define WHP_KEY_Q                  81
#define WHP_KEY_R                  82
#define WHP_KEY_S                  83
#define WHP_KEY_T                  84
#define WHP_KEY_U                  85
#define WHP_KEY_V                  86
#define WHP_KEY_W                  87
#define WHP_KEY_X                  88
#define WHP_KEY_Y                  89
#define WHP_KEY_Z                  90
#define WHP_KEY_LEFT_BRACKET       91  /* [ */
#define WHP_KEY_BACKSLASH          92  /* \ */
#define WHP_KEY_RIGHT_BRACKET      93  /* ] */
#define WHP_KEY_GRAVE_ACCENT       96  /* ` */
#define WHP_KEY_WORLD_1            161 /* non-US #1 */
#define WHP_KEY_WORLD_2            162 /* non-US #2 */
		
/* Function keys */
#define WHP_KEY_ESCAPE             256
#define WHP_KEY_ENTER              257
#define WHP_KEY_TAB                258
#define WHP_KEY_BACKSPACE          259
#define WHP_KEY_INSERT             260
#define WHP_KEY_DELETE             261
#define WHP_KEY_RIGHT              262
#define WHP_KEY_LEFT               263
#define WHP_KEY_DOWN               264
#define WHP_KEY_UP                 265
#define WHP_KEY_PAGE_UP            266
#define WHP_KEY_PAGE_DOWN          267
#define WHP_KEY_HOME               268
#define WHP_KEY_END                269
#define WHP_KEY_CAPS_LOCK          280
#define WHP_KEY_SCROLL_LOCK        281
#define WHP_KEY_NUM_LOCK           282
#define WHP_KEY_PRINT_SCREEN       283
#define WHP_KEY_PAUSE              284
#define WHP_KEY_F1                 290
#define WHP_KEY_F2                 291
#define WHP_KEY_F3                 292
#define WHP_KEY_F4                 293
#define WHP_KEY_F5                 294
#define WHP_KEY_F6                 295
#define WHP_KEY_F7                 296
#define WHP_KEY_F8                 297
#define WHP_KEY_F9                 298
#define WHP_KEY_F10                299
#define WHP_KEY_F11                300
#define WHP_KEY_F12                301
#define WHP_KEY_F13                302
#define WHP_KEY_F14                303
#define WHP_KEY_F15                304
#define WHP_KEY_F16                305
#define WHP_KEY_F17                306
#define WHP_KEY_F18                307
#define WHP_KEY_F19                308
#define WHP_KEY_F20                309
#define WHP_KEY_F21                310
#define WHP_KEY_F22                311
#define WHP_KEY_F23                312
#define WHP_KEY_F24                313
#define WHP_KEY_F25                314
#define WHP_KEY_KP_0               320
#define WHP_KEY_KP_1               321
#define WHP_KEY_KP_2               322
#define WHP_KEY_KP_3               323
#define WHP_KEY_KP_4               324
#define WHP_KEY_KP_5               325
#define WHP_KEY_KP_6               326
#define WHP_KEY_KP_7               327
#define WHP_KEY_KP_8               328
#define WHP_KEY_KP_9               329
#define WHP_KEY_KP_DECIMAL         330
#define WHP_KEY_KP_DIVIDE          331
#define WHP_KEY_KP_MULTIPLY        332
#define WHP_KEY_KP_SUBTRACT        333
#define WHP_KEY_KP_ADD             334
#define WHP_KEY_KP_ENTER           335
#define WHP_KEY_KP_EQUAL           336
#define WHP_KEY_LEFT_SHIFT         340
#define WHP_KEY_LEFT_CONTROL       341
#define WHP_KEY_LEFT_ALT			342
#define WHP_KEY_LEFT_OS				343
#define WHP_KEY_RIGHT_SHIFT        344
#define WHP_KEY_RIGHT_CONTROL      345
#define WHP_KEY_RIGHT_ALT          346
#define WHP_KEY_RIGHT_OS	      347
#define WHP_KEY_MENU               348

#define WHP_KEY_LAST               WHP_KEY_MENU

#define WHP_MOD_SHIFT           WHP_BIT(0)
/*! @brief If this bit is set one or more Control keys were held down.
 *
 *  If this bit is set one or more Control keys were held down.
 */
#define WHP_MOD_CONTROL         WHP_BIT(1)
 /*! @brief If this bit is set one or more Alt keys were held down.
  *
  *  If this bit is set one or more Alt keys were held down.
  */
#define WHP_MOD_ALT             WHP_BIT(2)
  /*! @brief If this bit is set one or more Super keys were held down.
   *
   *  If this bit is set one or more Super keys were held down.
   */
#define WHP_MOD_SUPER           WHP_BIT(3)
   /*! @brief If this bit is set the Caps Lock key is enabled.
	*
	*  If this bit is set the Caps Lock key is enabled and the @ref
	*  GLFW_LOCK_KEY_MODS input mode is set.
	*/
#define WHP_MOD_CAPS_LOCK       WHP_BIT(4)
	/*! @brief If this bit is set the Num Lock key is enabled.
	 *
	 *  If this bit is set the Num Lock key is enabled and the @ref
	 *  GLFW_LOCK_KEY_MODS input mode is set.
	 */
#define WHP_MOD_NUM_LOCK        WHP_BIT(5)

#define WHP_KEY_RELEASE				0
#define WHP_KEY_PRESS				1
#define WHP_KEY_REPEAT				2

#endif // WHP_KEYCODES
