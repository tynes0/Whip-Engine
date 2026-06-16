#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/Timestep.h>


_WHIP_START

class ScriptGlue
{
public:
	static void RegisterComponents();
	static void RegisterFunctions();

	static void OnRuntimeStart();
	static void OnRuntimeStop();
};

_WHIP_END
