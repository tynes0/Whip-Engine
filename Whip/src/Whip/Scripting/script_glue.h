#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/Timestep.h>


_WHIP_START

class script_glue
{
public:
	static void register_components();
	static void register_functions();

	static void on_runtime_start();
	static void on_runtime_stop();
};

_WHIP_END
