#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Render/Texture.h>
#include "Asset.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

_WHIP_START

enum class TextureSpriteMode
{
	Single = 0,
	Multiple
};

struct TextureSpriteRect
{
	std::string m_Name = "Sprite";
	uint32_t m_X = 0;
	uint32_t m_Y = 0;
	uint32_t m_Width = 16;
	uint32_t m_Height = 16;
};

struct TextureImportSettings
{
	TextureFilterMode m_FilterMode = TextureFilterMode::Linear;
	TextureWrapMode m_WrapMode = TextureWrapMode::Repeat;
	bool m_GenerateMips = true;
	bool m_AlphaTransparency = true;
	TextureSpriteMode m_SpriteMode = TextureSpriteMode::Single;
	float m_PixelsPerUnit = 100.0f;
	std::vector<TextureSpriteRect> m_Sprites;
};

struct AssetMetadata
{
	AssetType m_Type = AssetType::None;
	std::filesystem::path m_Filepath;
	TextureImportSettings m_TextureSettings;

	operator bool() const { return m_Type != AssetType::None; }
};

_WHIP_END
