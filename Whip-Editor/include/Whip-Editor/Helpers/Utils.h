#pragma once

#include <Whip.h>

_WHIP_START

class EditorUtils
{
public:
	static bool PathIsOrIsUnder(const std::filesystem::path& path, const std::filesystem::path& directory);
	static bool IsControlDown();
};

_WHIP_END
