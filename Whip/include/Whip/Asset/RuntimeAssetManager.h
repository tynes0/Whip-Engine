#pragma once

#include "Whip/Core/Core.h"
#include "AssetMetadata.h"
#include "AssetManagerBase.h"
#include "EditorAssetManager.h"

_WHIP_START

class RuntimeAssetManager : public AssetManagerBase
{
public:
	Ref<Asset> GetAsset(AssetHandle handle) override;

	bool IsAssetHandleValid(AssetHandle handle) const override;
	bool IsAssetLoaded(AssetHandle handle) const override;
	AssetType GetAssetType(AssetHandle handle) const override;
	void AddRegistry(AssetHandle handle, const AssetMetadata& metadata) override;

	AssetHandle ImportAsset(const std::filesystem::path& filepath);
	void DeleteAsset(AssetHandle handle);

	const AssetMetadata& GetMetadata(AssetHandle handle) const override;
	const std::filesystem::path& GetFilepath(AssetHandle handle) const;
	const AssetRegistry& GetAssetRegistry() const { return m_AssetRegistry; }

	void RuntimeStop();
	void SetEditorAssetManager(const Ref<EditorAssetManager>& manager);
	void AddAssetCopy(AssetHandle handle, const Ref<Asset>& assetCopy);
private:
	bool IsRuntime(AssetHandle handle) const;

	AssetRegistry m_AssetRegistry;
	AssetMap m_LoadedAssets;
	Ref<EditorAssetManager> m_EditorAssetManager;
};

_WHIP_END
