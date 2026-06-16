#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Project/Project.h>

#include "AssetManagerBase.h"

_WHIP_START

class AssetManager
{
public:
	template <class T, std::enable_if_t<IsAssetV<T>, int> = 0>
	static Ref<T> GetAsset(AssetHandle handle)
	{
		Ref<Asset> localAsset = Project::GetActive()->GetAssetManager()->GetAsset(handle);
		return std::static_pointer_cast<T>(localAsset);
	}

	static bool IsAssetHandleValid(AssetHandle handle)
	{
		return Project::GetActive()->GetAssetManager()->IsAssetHandleValid(handle);
	}

	static bool IsAssetLoaded(AssetHandle handle)
	{
		return Project::GetActive()->GetAssetManager()->IsAssetLoaded(handle);
	}

	static AssetType GetAssetType(AssetHandle handle)
	{
		return Project::GetActive()->GetAssetManager()->GetAssetType(handle);
	}

	static const AssetMetadata& GetAssetMetadata(AssetHandle handle)
	{
		return Project::GetActive()->GetAssetManager()->GetMetadata(handle);
	}

	static void AddRegistry(AssetHandle handle, const AssetMetadata& metadata)
	{
		Project::GetActive()->GetAssetManager()->AddRegistry(handle, metadata);
	}
};

_WHIP_END
