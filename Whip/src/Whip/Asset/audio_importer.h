#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/memory.h>
#include <Whip/Audio/audio_source.h>

#include "asset.h"
#include "asset_metadata.h"

#include <filesystem>


_WHIP_START

class audio_importer
{
public:
	// asset_metadata filepath is relative to project asset directory
	static ref<audio_source> import_audio(asset_handle handle, const asset_metadata& metadata);

	// Reads file directly from filesystem
		// (i.e. path has to be relative / absolute to working directory)
	static  ref<audio_source> load_audio(const std::filesystem::path& path, asset_handle handle = asset_handle{});
};

_WHIP_END
