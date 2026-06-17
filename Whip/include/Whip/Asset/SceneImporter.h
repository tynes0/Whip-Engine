#pragma once

#include "Whip/Core/Core.h"
#include "Whip/Scene/Scene.h"

#include "Asset.h"
#include "AssetMetadata.h"


_WHIP_START

class SceneImporter
{
public:
	// AssetMetadata filepath is relative to Project Asset directory
	static Ref<Scene> ImportScene(AssetHandle handle, const AssetMetadata& metadata);

	static Ref<Scene> LoadScene(const std::filesystem::path& path, AssetHandle handle = AssetHandle{});

	static void SaveScene(const Ref<Scene>& scene, const std::filesystem::path& path);
};

_WHIP_END
