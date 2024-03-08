#include "whippch.h"
#include "Filesystem.h"

#include "StringOperations.h"

#ifdef WHP_PLATFORM_WINDOWS
#include <io.h> // _access
#endif

_WHIP_START

namespace filesystem
{

	// --------------------- FILE EXIST ---------------------

	bool exist(const std::string& filepath)
	{
#ifdef WHP_PLATFORM_WINDOWS
		return (_access(filepath.c_str(), 0) == 0);
#else
		FILE* f = fopen(filepath.c_str(), "r");
		if (f != NULL)
		{
			fclose(f);
			return true;
		}
		return false;
#endif // WHP_PLATFORM_WINDOWS
	}

	// --------------------- WRITE TO FILE -------------------

	void write_to_file(const std::string& filepath, const std::string& output, bool append, bool write_if_exist)
	{
		if (write_if_exist)
		{
			if (!exist(filepath))
			{
				WHP_CORE_WARN("File {0} does not exist.", filepath);
				return;
			}
		}
			std::ofstream out(filepath, (!append) ? std::ios::out : std::ios::app);
			if (out)
			{
				out << output;
				out.close();
			}
			else
				WHP_CORE_ERROR("write_to_file cannot open file '{0}'", filepath);
	}

	// --------------------- FILE READER ---------------------

	std::string file_reader::read_file(const std::string& filepath)
	{
		std::string result;
		std::ifstream in(filepath, std::ios::in | std::ios::binary);
		if (in)
		{
			in.seekg(0, std::ios::end);
			result.resize(in.tellg());
			in.seekg(0, std::ios::beg);
			in.read(&result[0], result.size());
			in.close();
		}
		else
		{
			WHP_CORE_ERROR("File reader cannot open file '{0}'", filepath);
		}
		return result;
	}

	std::string file_reader::operator()(const std::string& filepath)
	{
		return read_file(filepath);
	}

	// --------------------- FILE CREATOR ---------------------

	void file_creator::create(const std::string& filepath)
	{
		std::ofstream out(filepath);
		if (out)
			out.close();
		else
			WHP_CORE_ERROR("File creator cannot create file '{0}'", filepath);
	}

	void file_creator::create(const std::string& filepath, const std::string& filename, const std::string& extension)
	{
		if (!exist(filepath))
		{
			WHP_CORE_ERROR("File creator cannot open folder '{0}'", filepath);
			return;
		}
		std::string new_extension = (string_operations::starts_with(extension, '.')) ? extension : '.' + extension;
		std::string new_path = (string_operations::ends_with(filepath, '\\') || string_operations::ends_with(filepath, '/')) ? filepath : filepath + '\\';
		std::ofstream out(new_path + filename + new_extension);
		if (out)
			out.close();
		else
			WHP_CORE_ERROR("File creator cannot create file '{0}'", new_path + filename + new_extension);
	}

	void file_creator::operator()(const std::string& filepath)
	{
		create(filepath);
	}

	void file_creator::operator()(const std::string& filepath, const std::string& filename, const std::string& extension)
	{
		create(filepath, filename, extension);
	}

	// --------------------- FILE CLEANER ---------------------

	void file_cleaner::clear(const std::string& filepath)
	{
		std::ofstream ofile(filepath, std::ios::out | std::ios::trunc);
		if (ofile)
			ofile.close();
		else
			WHP_CORE_ERROR("File cleaner cannot open file '{0}'", filepath);
	}

	void file_cleaner::operator()(const std::string& filepath)
	{
		clear(filepath);
	}

	// --------------------- FILE SEPERATOR ---------------------

	vector<std::string> file_separator::seperate(const std::string& path, const std::string& token)
	{
		std::string source = file_reader::read_file(path);
		return string_separator::seperate(source, token);
	}

	WHP_NODISCARD vector<std::string> file_separator::seperate(const std::string& path, char token)
	{
		std::string source = file_reader::read_file(path);
		return string_separator::seperate(source, token);
	}

	vector<std::string> file_separator::operator()(const std::string& path, const std::string& token)
	{
		return seperate(path, token);
	}

	WHP_NODISCARD vector<std::string> file_separator::operator()(const std::string& path, char token)
	{
		return seperate(path, token);
	}

	// --------------------- FILENAME FETCHER ---------------

	std::string filename_fetcher::fetch(const std::string& filepath)
	{
		size_t last_slash = filepath.find_last_of("/\\");
		last_slash = (last_slash == std::string::npos) ? 0 : last_slash + 1;
		size_t last_dot = filepath.rfind('.');
		size_t length = (last_dot == std::string::npos) ? filepath.size() - last_slash : last_dot - last_slash;
		return filepath.substr(last_slash, length);
	}

	std::string filename_fetcher::operator()(const std::string& filepath)
	{
		return fetch(filepath);
	}

} // namespace filesystem

_WHIP_END