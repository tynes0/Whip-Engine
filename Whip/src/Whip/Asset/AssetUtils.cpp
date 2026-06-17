#include "WhipPch.h"
#include "Whip/Asset/AssetUtils.h"
#include "Whip/Utils/FileExtensions.h"

_WHIP_START
	namespace
{
	std::map<std::filesystem::path, AssetType> s_AssetExtensionsMap =
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
		{ FileExtensions::AnimationController, AssetType::AnimationController },
		{ FileExtensions::EntityTemplate, AssetType::Entity }
	};
}

AssetType Utils::TryGetAssetTypeFromFileExtension(const std::filesystem::path& extension)
{
	std::string normalizedExtension = extension.string();
	std::ranges::transform(normalizedExtension, normalizedExtension.begin(),
	                       [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
	auto it = s_AssetExtensionsMap.find(normalizedExtension);
	if (it == s_AssetExtensionsMap.end())
		return AssetType::None;

	return it->second;
}

AssetType Utils::GetAssetTypeFromFileExtension(const std::filesystem::path& extension)
{
	AssetType type = TryGetAssetTypeFromFileExtension(extension);
	if (type == AssetType::None)
		WHP_CORE_WARN("[Asset Manager] Could not find AssetType for {0}", extension.string());

	return type;
}

YAML::Emitter& operator<<(YAML::Emitter& out, const std::string_view& sv)
{
	out << std::string(sv.data(), sv.size());
	return out;
}

_WHIP_END
