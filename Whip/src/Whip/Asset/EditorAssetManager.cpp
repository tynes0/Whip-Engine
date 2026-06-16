#include "WhipPch.h"
#include <Whip/Asset/EditorAssetManager.h>

#include <Whip/Asset/AssetImporter.h>
#include <Whip/Asset/AssetUtils.h>

#include <Whip/Project/Project.h>

#include <fstream>
#include <vector>

_WHIP_START

namespace
{
	bool PathIsUnderDirectory(const std::filesystem::path& path, const std::filesystem::path& directory)
	{
		std::filesystem::path normalizedPath = path.lexically_normal();
		std::filesystem::path normalizedDirectory = directory.lexically_normal();
		if (normalizedPath == normalizedDirectory)
			return true;

		auto pathIt = normalizedPath.begin();
		auto directoryIt = normalizedDirectory.begin();
		for (; directoryIt != normalizedDirectory.end(); ++directoryIt, ++pathIt)
		{
			if (pathIt == normalizedPath.end() || *pathIt != *directoryIt)
				return false;
		}
		return true;
	}
}

Ref<Asset> EditorAssetManager::GetAsset(AssetHandle handle)
{
	if (!IsAssetHandleValid(handle))
		return nullptr;

	Ref<Asset> asset;

	if (IsAssetLoaded(handle))
	{
		asset = m_LoadedAssets.at(handle);
	}
	else
	{
		const AssetMetadata& metadata = GetMetadata(handle);
		asset = AssetImporter::ImportAsset(handle, metadata);
		if (!asset)
			WHP_CORE_ERROR("[Asset Manager] Asset import failed!");
		else
			m_LoadedAssets[handle] = asset;
	}
	return asset;
}

bool EditorAssetManager::IsAssetHandleValid(AssetHandle handle) const
{
	return m_AssetRegistry.Exist(handle);
}

bool EditorAssetManager::IsAssetLoaded(AssetHandle handle) const
{
	return m_LoadedAssets.find(handle) != m_LoadedAssets.end();
}

AssetType EditorAssetManager::GetAssetType(AssetHandle handle) const
{
	return m_AssetRegistry.TypeOf(handle);
}

const AssetMetadata& EditorAssetManager::GetMetadata(AssetHandle handle) const
{
	return m_AssetRegistry.Get(handle);
}

void EditorAssetManager::AddRegistry(AssetHandle handle, const AssetMetadata& metadata)
{
	m_AssetRegistry.Add(handle, metadata);
}

AssetHandle EditorAssetManager::ImportAsset(const std::filesystem::path& filepath)
{
	if (AssetHandle existingHandle = GetHandleFromFilepath(filepath); existingHandle != 0)
		return existingHandle;

	std::filesystem::path absolutePath = Project::GetActiveAssetDirectory() / filepath;
	if (!std::filesystem::exists(absolutePath))
	{
		WHP_CORE_WARN("[Asset Manager] Asset file does not exist: {0}", absolutePath.string());
		return 0;
	}

	AssetType type = Utils::TryGetAssetTypeFromFileExtension(filepath.extension());
	if (type == AssetType::None)
	{
		WHP_CORE_WARN("[Asset Manager] Unsupported Asset extension '{0}' for '{1}'", filepath.extension().string(), filepath.string());
		return 0;
	}

	AssetHandle handle; // generate new handle
	AssetMetadata metadata;
	metadata.m_Filepath = filepath;
	metadata.m_Type = type;
	Ref<Asset> asset = AssetImporter::ImportAsset(handle, metadata);
	if (asset)
	{
		asset->m_Handle = handle;
		m_LoadedAssets[handle] = asset;
		m_AssetRegistry.AddOrReset(handle, metadata);
		m_AssetRegistry.Serialize();
		return handle;
	}
	else
		WHP_CORE_ERROR("[Asset Manager] Asset import failed!");

	return 0;
}

void EditorAssetManager::DeleteAsset(AssetHandle handle)
{
	auto it = m_LoadedAssets.find(handle);
	if (it != m_LoadedAssets.end())
		m_LoadedAssets.erase(it);

	m_AssetRegistry.Remove(handle);
	m_AssetRegistry.Serialize();
}

void EditorAssetManager::SetLoadedAsset(AssetHandle handle, const Ref<Asset>& asset)
{
	if (!IsAssetHandleValid(handle) || !asset)
		return;

	asset->m_Handle = handle;
	m_LoadedAssets[handle] = asset;
}

void EditorAssetManager::UnloadAsset(AssetHandle handle)
{
	auto it = m_LoadedAssets.find(handle);
	if (it != m_LoadedAssets.end())
		m_LoadedAssets.erase(it);
}

bool EditorAssetManager::UpdateAssetFilepath(AssetHandle handle, const std::filesystem::path& filepath)
{
	if (!IsAssetHandleValid(handle))
		return false;

	m_AssetRegistry.Get(handle).m_Filepath = filepath.lexically_normal();
	m_AssetRegistry.Serialize();
	return true;
}

size_t EditorAssetManager::UpdateAssetDirectoryPaths(const std::filesystem::path& oldDirectory, const std::filesystem::path& newDirectory)
{
	size_t updatedCount = 0;
	const std::filesystem::path normalizedOldDirectory = oldDirectory.lexically_normal();
	const std::filesystem::path normalizedNewDirectory = newDirectory.lexically_normal();

	m_AssetRegistry.Foreach([&](AssetRegistry::ValueType& value)
		{
			if (!PathIsUnderDirectory(value.second.m_Filepath, normalizedOldDirectory))
				return;

			std::error_code error;
			std::filesystem::path relativeTail = std::filesystem::relative(value.second.m_Filepath, normalizedOldDirectory, error);
			if (error)
				return;

			value.second.m_Filepath = (normalizedNewDirectory / relativeTail).lexically_normal();
			++updatedCount;
		});

	if (updatedCount > 0)
		m_AssetRegistry.Serialize();
	return updatedCount;
}

size_t EditorAssetManager::DeleteAssetsUnderDirectory(const std::filesystem::path& directory)
{
	std::vector<AssetHandle> handles;
	const std::filesystem::path normalizedDirectory = directory.lexically_normal();
	m_AssetRegistry.Foreach([&](const AssetRegistry::ValueType& value)
		{
			if (PathIsUnderDirectory(value.second.m_Filepath, normalizedDirectory))
				handles.push_back(value.first);
		});

	for (AssetHandle handle : handles)
	{
		auto loadedIt = m_LoadedAssets.find(handle);
		if (loadedIt != m_LoadedAssets.end())
			m_LoadedAssets.erase(loadedIt);
		m_AssetRegistry.Remove(handle);
	}

	if (!handles.empty())
		m_AssetRegistry.Serialize();
	return handles.size();
}

AssetHandle EditorAssetManager::GetHandleFromFilepath(const std::filesystem::path& filepath) const
{
	AssetHandle handle = 0;
	m_AssetRegistry.ForeachChecked([&](const AssetRegistry::ValueType& value)
		{
			if (value.second.m_Filepath == filepath)
			{
				handle = value.first;
				return AssetRegistry::LoopStop;
			}

			return AssetRegistry::LoopContinue;
		});
	return handle;
}

const std::filesystem::path& EditorAssetManager::GetFilepath(AssetHandle handle) const
{
	return GetMetadata(handle).m_Filepath;
}

void EditorAssetManager::SerializeAssetRegistry()
{
	m_AssetRegistry.Serialize();
}

bool EditorAssetManager::DeserializeAssetRegistry()
{
	return m_AssetRegistry.Deserialize();
}

_WHIP_END
