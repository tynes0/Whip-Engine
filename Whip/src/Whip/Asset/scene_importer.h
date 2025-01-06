#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Scene/scene.h>

#include "asset.h"
#include "asset_metadata.h"


_WHIP_START

class scene_importer
{
public:
	// asset_metadata filepath is relative to project asset directory
	static ref<scene> import_scene(asset_handle handle, const asset_metadata& metadata);

	static ref<scene> load_scene(const std::filesystem::path& path, asset_handle handle = asset_handle{});

	static void save_scene(ref<scene> scne, const std::filesystem::path& path);
};

_WHIP_END
