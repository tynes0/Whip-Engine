#pragma once

#include "Whip/Core/Memory.h"

#include "Asset.h"
#include "AssetMetadata.h"

_WHIP_START

class EntityTemplateImporter
{
public:
	static Ref<Asset> ImportEntityTemplate(AssetHandle handle, const AssetMetadata& metadata);
};

_WHIP_END
