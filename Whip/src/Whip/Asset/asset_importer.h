#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/memory.h>

#include "asset_metadata.h"

_WHIP_START

class asset_importer
{
public:
	static ref<asset> import_asset(asset_handle handle, const asset_metadata& metadata);
};

_WHIP_END
