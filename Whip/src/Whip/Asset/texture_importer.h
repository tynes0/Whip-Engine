#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Render/Texture.h>

#include "asset.h"
#include "asset_metadata.h"

#include <filesystem>


_WHIP_START

class texture_importer
{
public:
	// asset_metadata filepath is relative to project asset directory
	static ref<texture2D> import_texture2D(asset_handle handle, const asset_metadata& metadata);

	// asset_metadata filepath is relative to project asset directory
	// Reads file directly from filesystem
	// (i.e. path has to be relative / absolute to working directory)
	static ref<texture2D> load_texture2D(const std::filesystem::path& path, flip_direction_t direction = flip_direction_none);
};

_WHIP_END
