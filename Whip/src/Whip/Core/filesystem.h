#pragma once

#include "core.h"
#include "buffer.h"

#include <filesystem>

_WHIP_START

	class filesystem
	{
	public:
		static raw_buffer read_file_binary(const std::filesystem::path& filepath);
	};

_WHIP_END
