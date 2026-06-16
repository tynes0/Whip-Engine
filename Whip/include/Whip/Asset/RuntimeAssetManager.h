#pragma once

#include <Whip/Core/Core.h>

#include "AssetMetadata.h"
#include "AssetManagerBase.h"
#include "EditorAssetManager.h"

_WHIP_START

class RuntimeAssetManager : public AssetManagerBase
{
public:
	virtual Ref<Asset> GetAsset(AssetHandle handle) override;

	virtual bool IsAssetHandleValid(AssetHandle handle) const override;
	virtual bool IsAssetLoaded(AssetHandle handle) const override;
	virtual AssetType GetAssetType(AssetHandle handle) const override;
	virtual void AddRegistry(AssetHandle handle, const AssetMetadata& metadata) override;

	AssetHandle ImportAsset(const std::filesystem::path& filepath);
	void DeleteAsset(AssetHandle handle);

	const AssetMetadata& GetMetadata(AssetHandle handle) const;
	const std::filesystem::path& GetFilepath(AssetHandle handle) const;
	const AssetRegistry& GetAssetRegistry() const { return m_AssetRegistry; }

	void RuntimeStop();
	void SetEditorAssetManager(Ref<EditorAssetManager> manager);
	void AddAssetCopy(AssetHandle handle, Ref<Asset> assetCopy);
private:
	bool IsRuntime(AssetHandle handle) const;

	AssetRegistry m_AssetRegistry;
	AssetMap m_LoadedAssets;
	Ref<EditorAssetManager> m_EditorAssetManager;
};

_WHIP_END
