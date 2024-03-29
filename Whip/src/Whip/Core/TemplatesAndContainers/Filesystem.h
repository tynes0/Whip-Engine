#pragma once
#ifndef _WHIP_FILESYSTEM_
#define _WHIP_FILESYSTEM_

#include "Whip/Core/Core.h"
#include <Whip/Core/TemplatesAndContainers/Vector.h>
#include <Whip/Core/TemplatesAndContainers/Pair.h>

#include <string>

#pragma warning(push)
#pragma warning(disable : _WHP_DISABLED_WARNINGS)

_WHIP_START

namespace filesystem
{

	// --------------------- FILE EXIST ----------------------
	WHP_NODISCARD bool exist(const std::string& filepath);

	// --------------------- WRITE TO FILE -------------------
	void write_to_file(const std::string& filepath, const std::string& output, bool append = false, bool write_if_exist = false);
	void write_to_file(FILE* file, const std::string& output, bool append = false);

	// --------------------- FILE READER ---------------------
	class file_reader
	{
	public:
		file_reader() {}
		~file_reader() {}
		// reads all of the file
		WHP_NODISCARD static std::string read_file(const std::string& filepath);
		WHP_NODISCARD static std::string read_file(FILE* file);
		WHP_NODISCARD std::string operator()(const std::string& filepath);
		WHP_NODISCARD std::string operator()(FILE* file);
	};

	// --------------------- FILE CREATOR ---------------------
	class file_creator
	{
	public:
		file_creator() {}
		~file_creator() {}

		static void create(const std::string& filepath);
		static void create(const std::string& filepath, const std::string& filename, const std::string& extension);
		void operator()(const std::string& filepath);
		void operator()(const std::string& filepath, const std::string& filename, const std::string& extension);
	};

	// --------------------- FILE CLEANER ---------------------
	class file_cleaner
	{
	public:
		file_cleaner() {}
		~file_cleaner() {}
		static void clear(const std::string& filepath);
		void operator()(const std::string& filepath);
	};

	// --------------------- FILE SEPERATOR ---------------------
	class file_separator
	{
	public:
		file_separator() {}
		~file_separator() {}
		WHP_NODISCARD static vector<std::string> seperate(const std::string& path, const std::string& token);
		WHP_NODISCARD static vector<std::string> seperate(const std::string& path, char token);
		WHP_NODISCARD vector<std::string> operator()(const std::string& path, const std::string& token);
		WHP_NODISCARD vector<std::string> operator()(const std::string& path, char token);
	};

	// --------------------- FILEPATH PARSER ---------------------
	class filepath_parser
	{
	public:
		WHP_NODISCARD static std::string fetch_filename(const std::string& filepath);
		WHP_NODISCARD static std::string fetch_extension(const std::string& filepath, bool without_dot = false);
		// first -> file name, second -> file extension
		WHP_NODISCARD static pair<std::string> fetch_name_n_extension(const std::string& filepath);
		// first -> filepath without file name and extension, second -> file name, third -> file extension
		WHP_NODISCARD static trio<std::string> fetch_all(const std::string& filepath);
	};

} // namespace filesystem

_WHIP_END

#pragma warning(pop)

#endif // !_WHIP_FILESYSTEM_