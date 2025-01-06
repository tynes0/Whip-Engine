#pragma once

#include <Whip/Core/Core.h>

#include <string>
#include <filesystem>

_WHIP_START

class file_dialogs
{
public:
	static std::string open_file(const char* filter, const char* root = "");
	static std::string save_file(const char* filter, const char* root = "");
	static std::string open_folder();
	static std::string open_folder_under_a_spesific_directory(const std::filesystem::path& root);
};

class time
{
public:
	static float get_time();
};

namespace utils
{
	void restart_program();
	std::string wstring_to_utf8(const std::wstring& wstr);
}

_WHIP_END
