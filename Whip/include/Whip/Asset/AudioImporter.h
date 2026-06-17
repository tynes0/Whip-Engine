#pragma once

#include "Whip/Core/Core.h"
#include "Whip/Core/Memory.h"
#include "Whip/Audio/AudioSource.h"

#include "Asset.h"
#include "AssetMetadata.h"

#include <filesystem>


_WHIP_START

class AudioImporter
{
public:
	// AssetMetadata filepath is relative to Project Asset directory
	static Ref<AudioSource> ImportAudio(AssetHandle handle, const AssetMetadata& metadata);

	// Reads file directly from filesystem
		// (i.e. path has to be relative / absolute to working directory)
	static  Ref<AudioSource> LoadAudio(const std::filesystem::path& path, AssetHandle handle = AssetHandle{});
};

_WHIP_END
