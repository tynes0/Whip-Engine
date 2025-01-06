#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/memory.h>
#include <Whip/Animation/animation2D.h>

#include "asset.h"
#include "asset_metadata.h"

_WHIP_START

class animation_importer
{
public:
	static ref<animation2D> import_animation(asset_handle handle, const asset_metadata& metadata);
	static ref<animation2D> load_animation(const std::filesystem::path& path, asset_handle handle = asset_handle{});
};

_WHIP_END
