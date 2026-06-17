#include "WhipPch.h"
#include "Whip/Asset/AssetManager.h"

_WHIP_START

bool AssetManager::IsAssetHandleValid(AssetHandle handle)
{
	return Project::GetActive()->GetAssetManager()->IsAssetHandleValid(handle);
}

bool AssetManager::IsAssetLoaded(AssetHandle handle)
{
	return Project::GetActive()->GetAssetManager()->IsAssetLoaded(handle);
}

AssetType AssetManager::GetAssetType(AssetHandle handle)
{
	return Project::GetActive()->GetAssetManager()->GetAssetType(handle);
}

const AssetMetadata& AssetManager::GetAssetMetadata(AssetHandle handle)
{
	return Project::GetActive()->GetAssetManager()->GetMetadata(handle);
}

void AssetManager::AddRegistry(AssetHandle handle, const AssetMetadata& metadata)
{
	Project::GetActive()->GetAssetManager()->AddRegistry(handle, metadata);
}

_WHIP_END
