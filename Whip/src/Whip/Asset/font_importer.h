#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/Memory.h>
#include <Whip/Render/font.h>

#include "asset_metadata.h"

#include <filesystem>


_WHIP_START

class font_importer
{
public:
	// asset_metadata filepath is relative to project asset directory
	static ref<font> import_font(asset_handle handle, const asset_metadata& metadata);

	// Reads file directly from filesystem
		// (i.e. path has to be relative / absolute to working directory)
	static ref<font> load_font(const std::filesystem::path& path, asset_handle handle = asset_handle{});
};

_WHIP_END
