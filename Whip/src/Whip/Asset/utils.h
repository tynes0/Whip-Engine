#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/Log.h>

#include "asset.h"

#include <map>
#include <filesystem>


#ifndef YAML_CPP_STATIC_DEFINE
#define YAML_CPP_STATIC_DEFINE
#endif // !YAML_CPP_STATIC_DEFINE
#include <yaml-cpp/yaml.h>

_WHIP_START

namespace utils
{
	static std::map<std::filesystem::path, asset_type> s_asset_extensions_map =
	{
		{ ".whip", asset_type::scene },
		{ ".png", asset_type::texture2D },
		{ ".jpg", asset_type::texture2D },
		{ ".jpeg", asset_type::texture2D },
		{ ".mp3", asset_type::audio },
		{ ".ogg", asset_type::audio },
		{ ".ttf", asset_type::font },
		{ ".wanim", asset_type::animation },
		{ ".went", asset_type::entity }
	};

	static asset_type get_asset_type_from_file_extension(const std::filesystem::path& extension)
	{
		if (s_asset_extensions_map.find(extension) == s_asset_extensions_map.end())
		{
			WHP_CORE_WARN("[Asset Manager] Could not find asset_type for {0}", extension.string());
			return asset_type::none;
		}
		return s_asset_extensions_map.at(extension);
	}
}

static YAML::Emitter& operator<<(YAML::Emitter& out, const std::string_view& sv)
{
	out << std::string(sv.data(), sv.size());
	return out;
}

_WHIP_END
