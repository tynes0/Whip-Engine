#pragma once

#include "Whip/Core/Core.h"
#include "Whip/Core/Memory.h"

#include "AssetMetadata.h"
#include "Asset.h"

#include <unordered_map>

_WHIP_START

using AssetMap = std::unordered_map<AssetHandle, Ref<Asset>>;

class AssetManagerBase  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~AssetManagerBase() = default;

	virtual Ref<Asset> GetAsset(AssetHandle handle) = 0;

	virtual bool IsAssetHandleValid(AssetHandle handle) const = 0;
	virtual bool IsAssetLoaded(AssetHandle handle) const = 0;
	virtual AssetType GetAssetType(AssetHandle handle) const = 0;
	virtual const AssetMetadata& GetMetadata(AssetHandle handle) const = 0;
	virtual void AddRegistry(AssetHandle handle, const AssetMetadata& metadata) = 0;
};

_WHIP_END
