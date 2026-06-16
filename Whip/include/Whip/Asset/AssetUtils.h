#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/Log.h>
#include <Whip/Utils/FileExtensions.h>

#include "Asset.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <filesystem>


#ifndef YAML_CPP_STATIC_DEFINE
#define YAML_CPP_STATIC_DEFINE
#endif // !YAML_CPP_STATIC_DEFINE
#include <yaml-cpp/yaml.h>

_WHIP_START

namespace Utils
{
	static std::map<std::filesystem::path, AssetType> s_AssetExtensionsMap =
	{
		{ FileExtensions::Scene, AssetType::Scene },
		{ FileExtensions::SceneLegacy, AssetType::Scene },
		{ ".png", AssetType::Texture2D },
		{ ".jpg", AssetType::Texture2D },
		{ ".jpeg", AssetType::Texture2D },
		{ ".mp3", AssetType::Audio },
		{ ".ogg", AssetType::Audio },
		{ ".ttf", AssetType::Font },
		{ FileExtensions::Animation, AssetType::Animation },
		{ FileExtensions::EntityTemplate, AssetType::Entity }
	};

	static AssetType TryGetAssetTypeFromFileExtension(const std::filesystem::path& extension)
	{
		std::string normalizedExtension = extension.string();
		std::transform(normalizedExtension.begin(), normalizedExtension.end(), normalizedExtension.begin(),
			[](unsigned char character) { return static_cast<char>(std::tolower(character)); });
		auto it = s_AssetExtensionsMap.find(normalizedExtension);
		if (it == s_AssetExtensionsMap.end())
			return AssetType::None;

		return it->second;
	}

	static AssetType GetAssetTypeFromFileExtension(const std::filesystem::path& extension)
	{
		AssetType type = TryGetAssetTypeFromFileExtension(extension);
		if (type == AssetType::None)
			WHP_CORE_WARN("[Asset Manager] Could not find AssetType for {0}", extension.string());

		return type;
	}
}

static YAML::Emitter& operator<<(YAML::Emitter& out, const std::string_view& sv)
{
	out << std::string(sv.data(), sv.size());
	return out;
}

_WHIP_END
