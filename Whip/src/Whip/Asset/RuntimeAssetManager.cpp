#include <WhipPch.h>
#include <Whip/Asset/RuntimeAssetManager.h> 

#include <Whip/Asset/AssetImporter.h>
#include <Whip/Asset/AssetUtils.h>

#include <fstream>

_WHIP_START

Ref<Asset> RuntimeAssetManager::GetAsset(AssetHandle handle)
{
	if (!IsRuntime(handle))
		return m_EditorAssetManager->GetAsset(handle);

	if (IsAssetLoaded(handle))
		return m_LoadedAssets.at(handle);
	const AssetMetadata& metadata = GetMetadata(handle);
	Ref<Asset> asset = AssetImporter::ImportAsset(handle, metadata);
	if (!asset)
		WHP_CORE_ERROR("[Asset Manager] Asset import failed!");
	m_LoadedAssets[handle] = asset;
	return asset;
}

bool RuntimeAssetManager::IsAssetHandleValid(AssetHandle handle) const
{
	if (m_AssetRegistry.Exist(handle))
		return true;
	return m_EditorAssetManager->IsAssetHandleValid(handle);
}

bool RuntimeAssetManager::IsAssetLoaded(AssetHandle handle) const
{
	if (m_LoadedAssets.find(handle) != m_LoadedAssets.end())
		return true;
	return m_EditorAssetManager->IsAssetLoaded(handle);
}

AssetType RuntimeAssetManager::GetAssetType(AssetHandle handle) const
{
	if (IsRuntime(handle))
		return m_AssetRegistry.TypeOf(handle);
	return m_EditorAssetManager->GetAssetType(handle);
}

void RuntimeAssetManager::AddRegistry(AssetHandle handle, const AssetMetadata& metadata)
{
	m_AssetRegistry.Add(handle, metadata);
}

AssetHandle RuntimeAssetManager::ImportAsset(const std::filesystem::path& filepath)
{
	AssetHandle handle; // generate new handle
	AssetMetadata metadata;
	metadata.m_Filepath = filepath;
	metadata.m_Type = Utils::GetAssetTypeFromFileExtension(filepath.extension());
	Ref<Asset> asset = AssetImporter::ImportAsset(handle, metadata);
	if (asset)
	{
		asset->m_Handle = handle;
		m_LoadedAssets[handle] = asset;
		m_AssetRegistry.AddOrReset(handle, metadata);
	}
	return handle;
}

void RuntimeAssetManager::DeleteAsset(AssetHandle handle)
{
	m_AssetRegistry.Remove(handle);
	auto it = m_LoadedAssets.find(handle);
	if (it != m_LoadedAssets.end())
		m_LoadedAssets.erase(it);
}

const AssetMetadata& RuntimeAssetManager::GetMetadata(AssetHandle handle) const
{
	auto it = m_AssetRegistry.Find(handle);
	if (m_AssetRegistry.IsNullIt(it))
		return m_EditorAssetManager->GetMetadata(handle);
	return it->second;
}

const std::filesystem::path& RuntimeAssetManager::GetFilepath(AssetHandle handle) const
{
	return GetMetadata(handle).m_Filepath;
}

void RuntimeAssetManager::RuntimeStop()
{
	m_AssetRegistry.Clear();
	m_LoadedAssets.clear();
}

void RuntimeAssetManager::SetEditorAssetManager(Ref<EditorAssetManager> manager)
{
	m_EditorAssetManager = manager;
}

void RuntimeAssetManager::AddAssetCopy(AssetHandle handle, Ref<Asset> assetCopy)
{
	if (IsAssetHandleValid(handle))
	{
		m_AssetRegistry.AddOrReset(assetCopy->m_Handle, GetMetadata(handle));
		m_LoadedAssets[assetCopy->m_Handle] = assetCopy;
	}
}

bool RuntimeAssetManager::IsRuntime(AssetHandle handle) const
{
	return m_AssetRegistry.Exist(handle);
}

_WHIP_END
