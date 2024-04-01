#pragma once
#ifndef _WHIP_FILESYSTEM_
#define _WHIP_FILESYSTEM_

#include "Whip/Core/Core.h"
#include "Vector.h"
#include "Pair.h"

#include <string>

#pragma warning(push)
#pragma warning(disable : _WHP_DISABLED_WARNINGS)

_WHIP_START

namespace filesystem
{
	WHP_NODISCARD bool exist(const std::string& filepath);
	void write_to_file(const std::string& filepath, const std::string& output, bool append = false, bool write_if_exist = false);
	void write_to_file(FILE* file, const std::string& output, bool append = false);
	WHP_NODISCARD std::string read_file(const std::string& filepath);
	WHP_NODISCARD std::string read_file(FILE* file);
	// DO NOT FORGET TO CLOSE FILE!
	FILE* create_file(const std::string& filepath);
	// DO NOT FORGET TO CLOSE FILE!
	FILE* create_file(const std::string& filepath, const std::string& filename, const std::string& extension);
	void clear_file_contents(const std::string& filepath);
	WHP_NODISCARD vector<std::string> separate_file_contents(const std::string& path, const std::string& token);
	WHP_NODISCARD vector<std::string> separate_file_contents(const std::string& path, char token);
	WHP_NODISCARD std::string fetch_filename(const std::string& filepath);
	WHP_NODISCARD std::string fetch_file_extension(const std::string& filepath, bool without_dot = false);
	// first -> file name, second -> file extension
	WHP_NODISCARD pair<std::string> fetch_filename_and_extension(const std::string& filepath);
	// first -> filepath without file name and extension, second -> file name, third -> file extension
	WHP_NODISCARD trio<std::string> fetch_all(const std::string& filepath);

} // namespace filesystem

_WHIP_END

#pragma warning(pop)

#endif // !_WHIP_FILESYSTEM_