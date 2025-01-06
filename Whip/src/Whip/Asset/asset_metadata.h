#pragma once

#include <Whip/Core/Core.h>
#include "asset.h"

#include <filesystem>

_WHIP_START

struct asset_metadata
{
	asset_type type = asset_type::none;
	std::filesystem::path filepath;

	operator bool() const { return type != asset_type::none; }
};

_WHIP_END
