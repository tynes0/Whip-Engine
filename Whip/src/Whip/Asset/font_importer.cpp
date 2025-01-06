#include "whippch.h"
#include "font_importer.h"

#include <Whip/Project/project.h>

_WHIP_START

ref<font> font_importer::import_font(asset_handle handle, const asset_metadata& metadata)
{
	return load_font(project::get_active_asset_directory() / metadata.filepath, handle);
}

ref<font> font_importer::load_font(const std::filesystem::path& path, asset_handle handle)
{
	return make_ref<font>(path, handle);
}

_WHIP_END
