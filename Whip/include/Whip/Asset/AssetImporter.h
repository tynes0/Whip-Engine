#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/Memory.h>

#include "AssetMetadata.h"

_WHIP_START

class AssetImporter
{
public:
	static Ref<Asset> ImportAsset(AssetHandle handle, const AssetMetadata& metadata);
};

_WHIP_END
