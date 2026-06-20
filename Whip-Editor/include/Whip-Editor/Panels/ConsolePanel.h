#pragma once

#include <Whip.h>

_WHIP_START

class ConsolePanel
{
public:
	static void Initialize();
	static void Shutdown();
	static void OnImGuiRender();
	static void SetOpen(bool open);
	static bool IsOpen();
	static bool ConsumeOpenDirty();
};

_WHIP_END
