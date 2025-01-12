#pragma once

#include "Core.h"

#include <type_traits>
#include <string>

_WHIP_START

template <class _Ty, std::enable_if_t<std::is_unsigned_v<_Ty>, int> = 0>
static constexpr _Ty npos = static_cast<_Ty>(-1);

namespace utils
{
	static std::string read_file(const std::string& filepath)
	{
		std::string result;
		std::ifstream istr(filepath, std::ios::in | std::ios::binary);
		if (!istr)
		{
			WHP_CORE_WARN("[Core] File reading failed!");
			return std::string();
		}
		istr.seekg(0, std::ios::end);
		result.resize(istr.tellg());
		istr.seekg(0, std::ios::beg);
		istr.read(result.data(), result.size());
		return result;
	}

	static std::string fetch_filename(const std::string& filepath)
	{
		size_t last_slash = filepath.find_last_of("/\\");
		last_slash = (last_slash == std::string::npos) ? 0 : last_slash + 1;
		size_t last_dot = filepath.rfind('.');
		size_t length = (last_dot == std::string::npos) ? filepath.size() - last_slash : last_dot - last_slash;
		return filepath.substr(last_slash, length);
	}
}

_WHIP_END
