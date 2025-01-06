#include "whippch.h"
#include "asset_importer.h"

#include "texture_importer.h"
#include "scene_importer.h"
#include "font_importer.h"
#include "audio_importer.h"
#include "animation_importer.h"

#include <map>
#include <functional>

_WHIP_START

using asset_import_function = std::function<ref<asset>(asset_handle, const asset_metadata&)>;

static std::map<asset_type, asset_import_function> s_asset_import_functions = 
{
		{ asset_type::texture2D, texture_importer::import_texture2D },
		{ asset_type::scene, scene_importer::import_scene },
		{ asset_type::audio, audio_importer::import_audio },
		{ asset_type::font, font_importer::import_font },
		{ asset_type::animation, animation_importer::import_animation }
};


ref<asset> asset_importer::import_asset(asset_handle handle, const asset_metadata& metadata)
{
	if (s_asset_import_functions.find(metadata.type) == s_asset_import_functions.end())
	{
		WHP_CORE_ERROR("[Asset Manager] No importer available for asset type {}", asset_type_to_string(metadata.type));
		return nullptr;
	}
	return s_asset_import_functions.at(metadata.type)(handle, metadata);
}

_WHIP_END
