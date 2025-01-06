#pragma once

#include <Whip.h>

#include <atomic>

_WHIP_START

class consol_panel 
{
public:
	static void initialize();
	static void shutdown();
	static void render_imgui_console();
};


_WHIP_END
