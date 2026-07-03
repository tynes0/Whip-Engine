#include <Whip-Editor/Helpers/Utils.h>

_WHIP_START

bool EditorUtils::PathIsOrIsUnder(const std::filesystem::path& path, const std::filesystem::path& directory)
{
	const std::filesystem::path normalizedPath = path.lexically_normal();
	const std::filesystem::path normalizedDirectory = directory.lexically_normal();
	if (normalizedPath == normalizedDirectory)
		return true;

	auto pathIt = normalizedPath.begin();
	auto directoryIt = normalizedDirectory.begin();
	for (; directoryIt != normalizedDirectory.end(); ++directoryIt, ++pathIt)
	{
		if (pathIt == normalizedPath.end() || *pathIt != *directoryIt)
			return false;
	}

	return true;
}

bool EditorUtils::IsControlDown()
{
	return Input::IsKeyDown(Key::LeftControl) || Input::IsKeyDown(Key::RightControl);
}

_WHIP_END
