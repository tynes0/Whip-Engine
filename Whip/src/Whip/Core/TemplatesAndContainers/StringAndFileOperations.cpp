#include "whippch.h"
#include "StringAndFileOperations.h"

_WHIP_START


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

// --------------------- STRING SEPERATOR ---------------------

std::vector<std::string> string_separator::seperate(const std::string& source, const std::string& token)
{
	std::vector<std::string> result;
	// position initialized with 0
	size_t pos = 0;
	// last_location initialized with 0
	size_t last_location = 0;
	// while loop will run if this run value equals true
	bool run = true;
	while (run)
	{
		pos = source.find(token, last_location);
		if (pos == std::string::npos)
		{
			// position sets end of the source
			pos = source.size();
			// last loop
			run = false;
		}
		// add to vector
		result.push_back(source.substr(last_location, pos - last_location));
		last_location = pos + token.length();
	}
	return result;
}

WHP_NODISCARD std::vector<std::string> string_separator::seperate(const std::string& source, char token)
{
	std::string temp = "";
	std::vector<std::string> result;

	for (int i = 0; i < (int)source.size(); i++)
	{
		if (source[i] != token)
		{
			temp += source[i];
		}
		else
		{
			result.push_back(temp);
			temp = "";
		}
	}
	result.push_back(temp);

	return result;
}

std::vector<std::string> string_separator::operator()(const std::string& path, const std::string& token)
{
	return seperate(path, token);
}

WHP_NODISCARD std::vector<std::string> string_separator::operator()(const std::string& path, char token)
{
	return seperate(path, token);
}

// --------------------- FILE SEPERATOR ---------------------

std::vector<std::string> file_separator::seperate(const std::string& path, const std::string& token)
{
	std::string source = file_reader::read_file(path);
	return string_separator::seperate(source, token);
}

WHP_NODISCARD std::vector<std::string> file_separator::seperate(const std::string& path, char token)
{
	std::string source = file_reader::read_file(path);
	return string_separator::seperate(source, token);
}

std::vector<std::string> file_separator::operator()(const std::string& path, const std::string& token)
{
	return seperate(path, token);
}

WHP_NODISCARD std::vector<std::string> file_separator::operator()(const std::string& path, char token)
{
	return seperate(path, token);
}

// --------------------- FILENAME FETCHER ---------------

std::string filename_fetcher::fetch(const std::string& filepath)
{
#if _HAS_CXX17
	std::filesystem::path fspath(filepath);
	return fspath.stem().string();
#else // !_HAS_CXX17
	size_t last_slash = filepath.find_last_of("/\\");
	last_slash = (last_slash == std::string::npos) ? 0 : last_slash + 1;
	size_t last_dot = filepath.rfind('.');
	size_t length = (last_dot == std::string::npos) ? filepath.size() - last_slash : last_dot - last_slash;
	return filepath.substr(last_slash, length);
#endif // _HAS_CXX17
}

std::string filename_fetcher::operator()(const std::string& filepath)
{
	return fetch(filepath);
}

// --------------------- STRING OPERATIONS ---------------------

std::string string_operations::trim_left(const std::string& str)
{
	std::string result(str);
	size_t i = result.find_first_not_of(' ');
	if (i != std::string::npos)
		result.erase(result.begin(), result.begin() + i);
	return result;
}

size_t string_operations::trim_left_directly(std::string& str)
{
	size_t i = str.find_first_not_of(' ');
	if (i != std::string::npos)
		str.erase(str.begin(), str.begin() + i);
	return str.size();
}

std::string string_operations::trim_right(const std::string& str)
{
	std::string result(str);
	size_t i = result.find_last_not_of(' ');
	if (i != std::string::npos)
		result.erase(result.begin() + i + 1, result.end());
	return result;
}

size_t string_operations::trim_right_directly(std::string& str)
{
	size_t i = str.find_last_not_of(' ');
	if (i != std::string::npos)
		str.erase(str.begin() + i + 1, str.end());
	return str.size();
}

std::string string_operations::trim(const std::string& str)
{
	return trim_left(trim_right(str));
}

size_t string_operations::trim_directly(std::string& str)
{
	trim_left_directly(str);
	return trim_right_directly(str);
}

std::string string_operations::chop(std::string& str, size_t offset)
{
	std::string result = "";
	if (offset >= str.size())
		return result;
	result.resize(offset);
	std::copy(str.begin(), str.begin() + offset, &result[0]);
	str.erase(str.begin(), str.begin() + offset);
	return result;
}

std::string string_operations::chop_but_left(std::string& str, size_t offset)
{
	std::string result = "";
	if (offset >= str.size())
		return result;
	result.resize(str.size() - offset);
	std::copy(str.begin() + offset, str.end(), &result[0]);
	str.erase(str.begin() + offset, str.end());
	return result;
}

std::string string_operations::reverse(const std::string& str)
{
	const char* fp = str.data();
	const char* ep = str.data() + str.size() - 1;
	std::string result;
	result.resize(str.size());
	size_t i = 0;
	while (ep >= fp)
	{
		result[i++] = DREF(ep--);
		result[str.size() - i] = DREF(fp++);
	}
	return result;
}

std::string& string_operations::reverse_directly(std::string& str)
{
	char* fp = str.data();
	char* ep = str.data() + str.size() - 1;
	while (ep > fp)
	{
		char temp = DREF(fp);
		DREF(fp++) = DREF(ep);
		DREF(ep--) = temp;
	}
	return str;
}

std::string string_operations::reverse_in_place(const std::string& str, size_t offset, size_t length)
{
	if (offset + length > str.size())
		return str;
	const char* fp = str.data() + offset;
	const char* ep = str.data() + offset + length - 1;
	std::string result;
	result.resize(str.size());
	std::copy(str.begin(), str.begin() + offset, &result[0]);
	size_t i = offset;
	size_t j = offset + length - 1;
	while (fp <= ep)
	{
		result[i++] = DREF(ep--);
		result[j--] = DREF(fp++);
	}
	std::copy(str.begin() + offset + length, str.end(), &result[offset + length]);

	return result;
}

std::string& string_operations::reverse_in_place_directly(std::string& str, size_t offset, size_t length)
{
	if (offset + length > str.size())
		return str;
	char* fp = str.data() + offset;
	char* ep = str.data() + offset + length - 1;
	size_t i = offset;
	size_t j = offset + length - 1;
	while (fp < ep)
	{
		char temp = DREF(fp);
		DREF(fp++) = DREF(ep);
		DREF(ep--) = temp;
	}
	return str;
}

std::string string_operations::remove_prefix(const std::string& str, size_t length)
{
	std::string result = "";
	if (str.size() <= length)
		return result;
	result.resize(str.size() - length);
	std::copy(str.begin() + length, str.end(), result.data());
	return result;
}

std::string& string_operations::remove_prefix_directly(std::string& str, size_t length)
{
	if (str.size() <= length)
		return str;
	str.erase(str.begin(), str.begin() + length);
	return str;
}

std::string string_operations::remove_suffix(const std::string& str, size_t length)
{
	std::string result = "";
	if (str.size() <= length)
		return result;
	result.resize(str.size() - length);
	std::copy(str.begin(), str.end() - length, result.data());
	return result;
}

std::string& string_operations::remove_suffix_directly(std::string& str, size_t length)
{
	if (str.size() <= length)
		return str;
	str.erase(str.end() - length, str.end());
	return str;
}

std::string string_operations::to_lower(const std::string& str)
{
	std::string result = str;
	for (char& item : result)
		item = tolower(item);
	return result;
}

std::string& string_operations::to_lower_directly(std::string& str)
{
	for (char& item : str)
		item = tolower(item);
	return str;
}

std::string string_operations::to_lower_in_place(const std::string& str, size_t offset, size_t length)
{
	std::string result = str;
	if (str.size() < offset + length)
		return result;
	for (auto it = result.begin() + offset; it != result.begin() + offset + length; ++it)
		DREF(it) = tolower(DREF(it));
	return result;
}

std::string& string_operations::to_lower_in_place_directly(std::string& str, size_t offset, size_t length)
{
	if (str.size() < offset + length)
		return str;
	for (auto it = str.begin() + offset; it != str.begin() + offset + length; ++it)
		DREF(it) = tolower(DREF(it));
	return str;
}

std::string string_operations::to_upper(const std::string& str)
{
	std::string result = str;
	for (char& item : result)
		item = toupper(item);
	return result;
}

std::string& string_operations::to_upper_directly(std::string& str)
{
	for (char& item : str)
		item = toupper(item);
	return str;
}

std::string string_operations::to_upper_in_place(const std::string& str, size_t offset, size_t length)
{
	std::string result = str;
	if (str.size() < offset + length)
		return result;
	for (auto it = result.begin() + offset; it != result.begin() + offset + length; ++it)
		DREF(it) = toupper(DREF(it));
	return result;
}

std::string& string_operations::to_upper_in_place_directly(std::string& str, size_t offset, size_t length)
{
	if (str.size() < offset + length)
		return str;
	for (auto it = str.begin() + offset; it != str.begin() + offset + length; ++it)
		DREF(it) = toupper(DREF(it));
	return str;
}

std::string string_operations::filled_copy(const std::string& str, char ch)
{
	std::string result;
	result.resize(str.size());
	std::fill(result.begin(), result.end(), ch);
	return result;
}

std::string& string_operations::fill(std::string& str, char ch)
{
	std::fill(str.begin(), str.end(), ch);
	return str;
}

std::string string_operations::fill_in_place_copy(const std::string& str, char ch, size_t offset, size_t length)
{
	std::string result = str;
	if (str.size() < offset + length)
		return result;
	std::fill(result.begin() + offset, result.begin() + offset + length, ch);
	return result;
}

std::string& string_operations::fill_in_place(std::string& str, char ch, size_t offset, size_t length)
{
	if (str.size() < offset + length)
		return str;
	std::fill(str.begin() + offset, str.begin() + offset + length, ch);
	return str;
}

bool string_operations::is_alnum(const std::string& str)
{
	size_t i = 0;
	for (; i < str.size() && isalnum(str[i]); ++i);
	if (i == str.size())
		return true;
	return false;
}

bool string_operations::is_alpha(const std::string& str)
{
	size_t i = 0;
	for (; i < str.size() && isalpha(str[i]); ++i);
	if (i == str.size())
		return true;
	return false;
}

bool string_operations::is_digit(const std::string& str)
{
	size_t i = 0;
	for (; i < str.size() && isdigit(str[i]); ++i);
	if (i == str.size())
		return true;
	return false;
}

bool string_operations::is_space(const std::string& str)
{
	size_t i = 0;
	for (; i < str.size() && isspace(str[i]); ++i);
	if (i == str.size())
		return true;
	return false;
}

bool string_operations::is_lower(const std::string& str)
{
	size_t i = 0;
	for (; i < str.size() && islower(str[i]); ++i);
	if (i == str.size())
		return true;
	return false;
}

bool string_operations::is_upper(const std::string& str)
{
	size_t i = 0;
	for (; i < str.size() && isupper(str[i]); ++i);
	if (i == str.size())
		return true;
	return false;
}

bool string_operations::is_printable(const std::string& str)
{
	size_t i = 0;
	for (; i < str.size() && isprint(str[i]); ++i);
	if (i == str.size())
		return true;
	return false;
}

_WHIP_END