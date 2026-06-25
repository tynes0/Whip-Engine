#pragma once

#include <Whip/Core/Core.h>

#include <string>
#include <filesystem>

_WHIP_START

class FileDialogs
{
public:
	static std::string OpenFile(const char* filter, const char* root = "");
	static std::string SaveFile(const char* filter, const char* root = "");
	static std::string OpenFolder();
	static std::string OpenFolderUnderASpesificDirectory(const std::filesystem::path& root);
};

class Time
{
public:
	static float GetTime();
};

namespace Utils
{
	bool RestartProgram();
	void WaitForRestartParentIfNeeded();
	bool OpenExternalPath(const std::filesystem::path& path);
	bool OpenExternalPathWith(const std::filesystem::path& executable, const std::filesystem::path& path);
	std::string WStringToUtf8(const std::wstring& wideString);
}

_WHIP_END
