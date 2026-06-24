#pragma once

#include "Whip/Core/Core.h"
#include "Asset.h"
#include "AssetMetadata.h"

#include <filesystem>
#include <string_view>


#ifndef YAML_CPP_STATIC_DEFINE
#define YAML_CPP_STATIC_DEFINE
#endif // !YAML_CPP_STATIC_DEFINE
#include <yaml-cpp/yaml.h>

_WHIP_START

namespace Utils
{
	AssetType TryGetAssetTypeFromFileExtension(const std::filesystem::path& extension);
	AssetType GetAssetTypeFromFileExtension(const std::filesystem::path& extension);
	bool NormalizeTextureSprites(TextureImportSettings& settings, uint32_t textureWidth = 0, uint32_t textureHeight = 0, std::string_view fallbackName = "sprite");
}

static YAML::Emitter& operator<<(YAML::Emitter& out, const std::string_view& sv);

_WHIP_END
