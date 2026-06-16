#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/Memory.h>
#include <Whip/Animation/Animation2D.h>

#include "Asset.h"
#include "AssetMetadata.h"

_WHIP_START

class AnimationImporter
{
public:
	static Ref<Animation2D> ImportAnimation(AssetHandle handle, const AssetMetadata& metadata);
	static Ref<Animation2D> LoadAnimation(const std::filesystem::path& path, AssetHandle handle = AssetHandle{});
};

_WHIP_END
