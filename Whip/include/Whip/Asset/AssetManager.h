#pragma once

#include "Whip/Core/Core.h"
#include "Whip/Project/Project.h"

#include "AssetManagerBase.h"

_WHIP_START

class AssetManager
{
public:
	template <class T>
	static Ref<T> GetAsset(AssetHandle handle)
		requires (IsAssetV<T>)
	{
		Ref<Asset> localAsset = Project::GetActive()->GetAssetManager()->GetAsset(handle);
		return std::static_pointer_cast<T>(localAsset);
	}

	static bool IsAssetHandleValid(AssetHandle handle);
	static bool IsAssetLoaded(AssetHandle handle);
	static AssetType GetAssetType(AssetHandle handle);
	static const AssetMetadata& GetAssetMetadata(AssetHandle handle);
	static void AddRegistry(AssetHandle handle, const AssetMetadata& metadata);
};

_WHIP_END
