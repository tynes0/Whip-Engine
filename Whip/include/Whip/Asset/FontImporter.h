#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/Memory.h>
#include <Whip/Render/Font.h>

#include "AssetMetadata.h"

#include <filesystem>


_WHIP_START

class FontImporter
{
public:
	// AssetMetadata filepath is relative to Project Asset directory
	static Ref<Font> ImportFont(AssetHandle handle, const AssetMetadata& metadata);

	// Reads file directly from filesystem
		// (i.e. path has to be relative / absolute to working directory)
	static Ref<Font> LoadFont(const std::filesystem::path& path, AssetHandle handle = AssetHandle{});
};

_WHIP_END
