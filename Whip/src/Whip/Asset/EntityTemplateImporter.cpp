#include "WhipPch.h"
#include "Whip/Asset/EntityTemplateImporter.h"
#include "Whip/Asset/EntityTemplateAsset.h"

_WHIP_START

Ref<Asset> EntityTemplateImporter::ImportEntityTemplate(AssetHandle handle, const AssetMetadata& metadata)
{
	return MakeRef<EntityTemplateAsset>(handle, metadata.m_Filepath);
}

_WHIP_END
