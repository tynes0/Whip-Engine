#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Helper/Buffer.h>

#include <filesystem>

_WHIP_START

	class FileSystem
	{
	public:
		static RawBuffer ReadFileBinary(const std::filesystem::path& filepath);
	};

_WHIP_END
