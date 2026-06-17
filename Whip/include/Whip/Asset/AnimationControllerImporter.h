#pragma once

#include "Whip/Animation/AnimationController.h"
#include "Whip/Core/Core.h"
#include "Whip/Core/Memory.h"

#include "Asset.h"
#include "AssetMetadata.h"

_WHIP_START

class AnimationControllerImporter
{
public:
	static Ref<AnimationController> ImportAnimationController(AssetHandle handle, const AssetMetadata& metadata);
	static Ref<AnimationController> LoadAnimationController(const std::filesystem::path& path, AssetHandle handle = AssetHandle{});
};

_WHIP_END
