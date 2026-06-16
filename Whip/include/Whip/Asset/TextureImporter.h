#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Render/Texture.h>

#include "Asset.h"
#include "AssetMetadata.h"

#include <filesystem>


_WHIP_START

class TextureImporter
{
public:
	// AssetMetadata filepath is relative to Project Asset directory
	static Ref<Texture2D> ImportTexture2D(AssetHandle handle, const AssetMetadata& metadata);

	// AssetMetadata filepath is relative to Project Asset directory
	// Reads file directly from filesystem
	// (i.e. path has to be relative / absolute to working directory)
	static Ref<Texture2D> LoadTexture2D(const std::filesystem::path& path, FlipDirection direction = FlipDirectionNone);
};

_WHIP_END
