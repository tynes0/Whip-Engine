#pragma once

#include <Whip.h>

#include <string>
#include <vector>

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
	static bool IsShortcutContextActive();
	static void Clear();
	static void CopyVisible();
	static void FocusSearch();
	static void ClearFilters();
	static void ShowAllLevels();
	static void ShowWarningsAndErrors();
	static void ShowErrorsOnly();
	static void ToggleAutoScroll();
	static std::vector<std::string> GetRecentMessages(size_t maxCount = 8);
};

_WHIP_END
