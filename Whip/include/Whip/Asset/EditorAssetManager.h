#pragma once

#include <Whip/Core/Core.h>

#include "AssetMetadata.h"
#include "AssetManagerBase.h"
#include "AssetRegistry.h"

#include <map>
#include <filesystem>


_WHIP_START

class EditorAssetManager : public AssetManagerBase
{
public:
	virtual Ref<Asset> GetAsset(AssetHandle handle) override;

	virtual bool IsAssetHandleValid(AssetHandle handle) const override;
	virtual bool IsAssetLoaded(AssetHandle handle) const override;
	virtual AssetType GetAssetType(AssetHandle handle) const override;
	virtual const AssetMetadata& GetMetadata(AssetHandle handle) const override;
	virtual void AddRegistry(AssetHandle handle, const AssetMetadata& metadata) override;

	AssetHandle ImportAsset(const std::filesystem::path& filepath);
	void DeleteAsset(AssetHandle handle);
	void SetLoadedAsset(AssetHandle handle, const Ref<Asset>& asset);
	void UnloadAsset(AssetHandle handle);
	bool UpdateAssetFilepath(AssetHandle handle, const std::filesystem::path& filepath);
	size_t UpdateAssetDirectoryPaths(const std::filesystem::path& oldDirectory, const std::filesystem::path& newDirectory);
	size_t DeleteAssetsUnderDirectory(const std::filesystem::path& directory);

	AssetHandle GetHandleFromFilepath(const std::filesystem::path& filepath) const;
	const std::filesystem::path& GetFilepath(AssetHandle handle) const;
	const AssetRegistry& GetAssetRegistry() const { return m_AssetRegistry; }

	void SerializeAssetRegistry();
	bool DeserializeAssetRegistry();
private:
	AssetRegistry m_AssetRegistry;
	AssetMap m_LoadedAssets;

	// todo memory-only assets
};

_WHIP_END
