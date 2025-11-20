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
};


_WHIP_END
