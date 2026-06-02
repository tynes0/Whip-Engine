#pragma once

#include <Whip/Core/memory.h>

#include "asset.h"
#include "asset_metadata.h"

_WHIP_START

class entity_template_importer
{
public:
	static ref<asset> import_entity_template(asset_handle handle, const asset_metadata& metadata);
};

_WHIP_END
