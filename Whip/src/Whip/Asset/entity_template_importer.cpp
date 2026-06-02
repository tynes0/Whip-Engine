#include "whippch.h"
#include "entity_template_importer.h"

#include "entity_template_asset.h"

_WHIP_START

ref<asset> entity_template_importer::import_entity_template(asset_handle handle, const asset_metadata& metadata)
{
	return make_ref<entity_template_asset>(handle, metadata.filepath);
}

_WHIP_END
