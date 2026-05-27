#pragma once

#include <Whip.h>

#include <atomic>

_WHIP_START

class console_panel 
{
public:
	static void initialize();
	static void shutdown();
	static void on_imgui_render();
	static void set_open(bool open);
	static bool is_open();
	static bool consume_open_dirty();
};


_WHIP_END
