#include "WhipPch.h"
#include "Whip/Asset/AssetUtils.h"
#include "Whip/Utils/FileExtensions.h"

#include <algorithm>
#include <format>
#include <string>
#include <unordered_set>

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

	std::string MakeSpriteName(std::string_view fallbackName, size_t index)
	{
		std::string base(fallbackName.empty() ? "sprite" : fallbackName);
		for (char& character : base)
		{
			if (character == ' ' || character == '\\' || character == '/')
				character = '_';
		}
		return std::format("{}_{:03}", base, index);
	}
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

bool Utils::NormalizeTextureSprites(TextureImportSettings& settings, uint32_t textureWidth, uint32_t textureHeight, std::string_view fallbackName)
{
	bool changed = false;
	std::unordered_set<std::string> usedNames;
	for (size_t index = 0; index < settings.m_Sprites.size(); ++index)
	{
		TextureSpriteRect& sprite = settings.m_Sprites[index];
		if (sprite.m_Name.empty())
		{
			sprite.m_Name = MakeSpriteName(fallbackName, index);
			changed = true;
		}

		std::string uniqueName = sprite.m_Name;
		for (uint32_t duplicateIndex = 2; usedNames.contains(uniqueName); ++duplicateIndex)
			uniqueName = std::format("{} {}", sprite.m_Name, duplicateIndex);
		if (uniqueName != sprite.m_Name)
		{
			sprite.m_Name = std::move(uniqueName);
			changed = true;
		}
		usedNames.insert(sprite.m_Name);

		if (sprite.m_Width == 0)
		{
			sprite.m_Width = 1;
			changed = true;
		}
		if (sprite.m_Height == 0)
		{
			sprite.m_Height = 1;
			changed = true;
		}

		if (textureWidth > 0 && textureHeight > 0)
		{
			const uint32_t clampedX = std::min(sprite.m_X, textureWidth - 1);
			const uint32_t clampedY = std::min(sprite.m_Y, textureHeight - 1);
			const uint32_t clampedWidth = std::clamp(sprite.m_Width, 1u, textureWidth - clampedX);
			const uint32_t clampedHeight = std::clamp(sprite.m_Height, 1u, textureHeight - clampedY);
			if (sprite.m_X != clampedX || sprite.m_Y != clampedY || sprite.m_Width != clampedWidth || sprite.m_Height != clampedHeight)
			{
				sprite.m_X = clampedX;
				sprite.m_Y = clampedY;
				sprite.m_Width = clampedWidth;
				sprite.m_Height = clampedHeight;
				changed = true;
			}
		}
	}

	if (settings.m_Sprites.empty() && settings.m_SpriteMode != TextureSpriteMode::Single)
	{
		settings.m_SpriteMode = TextureSpriteMode::Single;
		changed = true;
	}
	else if (!settings.m_Sprites.empty() && settings.m_SpriteMode != TextureSpriteMode::Multiple)
	{
		settings.m_SpriteMode = TextureSpriteMode::Multiple;
		changed = true;
	}

	return changed;
}

YAML::Emitter& operator<<(YAML::Emitter& out, const std::string_view& sv)
{
	out << std::string(sv.data(), sv.size());
	return out;
}

_WHIP_END
