#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/Log.h>

#include <type_traits>
#include <string>
#include <fstream>

_WHIP_START

template <class T, std::enable_if_t<std::is_unsigned_v<T>, int> = 0>
inline constexpr T npos = static_cast<T>(-1);

namespace Utils
{
	inline std::string ReadFile(const std::string& filepath)
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

	inline std::string FetchFilename(const std::string& filepath)
	{
		size_t lastSlash = filepath.find_last_of("/\\");
		lastSlash = (lastSlash == std::string::npos) ? 0 : lastSlash + 1;
		size_t lastDot = filepath.rfind('.');
		size_t length = (lastDot == std::string::npos) ? filepath.size() - lastSlash : lastDot - lastSlash;
		return filepath.substr(lastSlash, length);
	}
}

_WHIP_END
