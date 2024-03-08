#pragma once

#include "Core.h"

#include <string>
#include <Whip/Core/TemplatesAndContainers/Vector.h>

_WHIP_START

namespace filesystem
{

	// --------------------- FILE EXIST ----------------------
	WHP_NODISCARD bool exist(const std::string& filepath);

	// --------------------- WRITE TO FILE -------------------
	void write_to_file(const std::string& filepath, const std::string& output, bool append = false, bool write_if_exist = false);

	// --------------------- FILE READER ---------------------
	class file_reader
	{
	public:
		file_reader() {}
		~file_reader() {}
		// reads all of the file
		WHP_NODISCARD static std::string read_file(const std::string& filepath);
		WHP_NODISCARD std::string operator()(const std::string& filepath);
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

	// --------------------- FILENAME FETCHER ---------------------
	class filename_fetcher
	{
	public:
		WHP_NODISCARD static std::string fetch(const std::string& filepath);
		WHP_NODISCARD std::string operator()(const std::string& filepath);
	};

	// ------------------ FILE EXTENSION FETCHER ------------------

	class file_extension_fetcher
	{
	public:
		WHP_NODISCARD static std::string fetch(const std::string& filepath);
		WHP_NODISCARD std::string& operator()(const std::string& filepath);
	};

} // namespace filesystem

_WHIP_END