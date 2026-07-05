#pragma once

#include "AssetMetadata.h"
#include "AssetManagerBase.h"
#include "AssetRegistry.h"

#include <filesystem>


_WHIP_START

class EditorAssetManager : public AssetManagerBase
{
public:
	Ref<Asset> GetAsset(AssetHandle handle) override;

	bool IsAssetHandleValid(AssetHandle handle) const override;
	bool IsAssetLoaded(AssetHandle handle) const override;
	AssetType GetAssetType(AssetHandle handle) const override;
	const AssetMetadata& GetMetadata(AssetHandle handle) const override;
	void AddRegistry(AssetHandle handle, const AssetMetadata& metadata) override;

	AssetHandle ImportAsset(const std::filesystem::path& filepath);
	void DeleteAsset(AssetHandle handle);
	void SetLoadedAsset(AssetHandle handle, const Ref<Asset>& asset);
	void UnloadAsset(AssetHandle handle);
	bool ReimportAsset(AssetHandle handle);
	bool UpdateAssetMetadata(AssetHandle handle, const AssetMetadata& metadata);
	bool UpdateAssetFilepath(AssetHandle handle, const std::filesystem::path& filepath);
	bool UpdateAssetFilepath(AssetHandle handle, const std::filesystem::path& filepath, const std::filesystem::path& registryPath);
	size_t UpdateAssetDirectoryPaths(const std::filesystem::path& oldDirectory, const std::filesystem::path& newDirectory);
	size_t DeleteAssetsUnderDirectory(const std::filesystem::path& directory);

	AssetHandle GetHandleFromFilepath(const std::filesystem::path& filepath) const;
	const std::filesystem::path& GetFilepath(AssetHandle handle) const;
	const AssetRegistry& GetAssetRegistry() const;

	void SerializeAssetRegistry(bool* result = nullptr);
	void SerializeAssetRegistry(const std::filesystem::path& registryPath, bool* result = nullptr);
	bool DeserializeAssetRegistry();
	bool DeserializeAssetRegistry(const std::filesystem::path& registryPath);
private:
	AssetRegistry m_AssetRegistry;
	AssetMap m_LoadedAssets;

	// todo memory-only assets
};

_WHIP_END
