#include "WhipPch.h"
#include "Whip/Asset/EditorAssetManager.h"
#include "Whip/Asset/AssetImporter.h"
#include "Whip/Asset/AssetUtils.h"
#include "Whip/Project/Project.h"

_WHIP_START

namespace
{
	std::string LowerCopy(std::string value)
	{
		std::ranges::transform(value, value.begin(),
			[](unsigned char character) { return static_cast<char>(std::tolower(character)); });
		return value;
	}

	std::filesystem::path NormalizeAssetPath(std::filesystem::path filepath)
	{
		if (filepath.empty())
			return {};

		if (filepath.is_absolute())
		{
			std::error_code error;
			filepath = std::filesystem::relative(filepath, Project::GetActiveAssetDirectory(), error);
			if (error)
				return {};
		}

		filepath = filepath.lexically_normal();
		if (filepath == ".")
			return {};
		return filepath;
	}

	bool AssetPathsEqual(const std::filesystem::path& left, const std::filesystem::path& right)
	{
		const std::string normalizedLeft = LowerCopy(left.lexically_normal().generic_string());
		const std::string normalizedRight = LowerCopy(right.lexically_normal().generic_string());
		return normalizedLeft == normalizedRight;
	}

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
	return m_LoadedAssets.contains(handle);
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
	const std::filesystem::path normalizedFilepath = NormalizeAssetPath(filepath);
	if (normalizedFilepath.empty())
		return 0;

	if (AssetHandle existingHandle = GetHandleFromFilepath(normalizedFilepath); existingHandle != 0)
		return existingHandle;

	std::filesystem::path absolutePath = Project::GetActiveAssetDirectory() / normalizedFilepath;
	if (!std::filesystem::exists(absolutePath))
	{
		WHP_CORE_WARN("[Asset Manager] Asset file does not exist: {0}", absolutePath.string());
		return 0;
	}

	AssetType type = Utils::TryGetAssetTypeFromFileExtension(normalizedFilepath.extension());
	if (type == AssetType::None)
	{
		WHP_CORE_WARN("[Asset Manager] Unsupported Asset extension '{0}' for '{1}'", normalizedFilepath.extension().string(), normalizedFilepath.string());
		return 0;
	}

	AssetHandle handle; // generate new handle
	AssetMetadata metadata;
	metadata.m_Filepath = normalizedFilepath;
	metadata.m_Type = type;
	if (Ref<Asset> asset = AssetImporter::ImportAsset(handle, metadata))
	{
		asset->m_Handle = handle;
		m_LoadedAssets[handle] = asset;
		m_AssetRegistry.AddOrReset(handle, metadata);
		SerializeAssetRegistry();
		return handle;
	}

	WHP_CORE_ERROR("[Asset Manager] Asset import failed!");
	return {0};
}

void EditorAssetManager::DeleteAsset(AssetHandle handle)
{
	if (!IsAssetHandleValid(handle))
		return;

	auto it = m_LoadedAssets.find(handle);
	if (it != m_LoadedAssets.end())
		m_LoadedAssets.erase(it);

	m_AssetRegistry.Remove(handle);
	SerializeAssetRegistry();
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

bool EditorAssetManager::ReimportAsset(AssetHandle handle)
{
	if (!IsAssetHandleValid(handle))
		return false;

	const AssetMetadata& metadata = GetMetadata(handle);
	Ref<Asset> asset = AssetImporter::ImportAsset(handle, metadata);
	if (!asset)
	{
		WHP_CORE_ERROR("[Asset Manager] Asset reimport failed!");
		return false;
	}

	asset->m_Handle = handle;
	m_LoadedAssets[handle] = asset;
	return true;
}

bool EditorAssetManager::UpdateAssetMetadata(AssetHandle handle, const AssetMetadata& metadata)
{
	if (!IsAssetHandleValid(handle))
		return false;

	AssetMetadata normalizedMetadata = metadata;
	if (normalizedMetadata.m_Type == AssetType::Texture2D)
		Utils::NormalizeTextureSprites(normalizedMetadata.m_TextureSettings, 0, 0, normalizedMetadata.m_Filepath.stem().string());

	if (!m_AssetRegistry.AddOrReset(handle, normalizedMetadata))
		return false;

	bool result = false;
	SerializeAssetRegistry(&result);
	return result;
}

bool EditorAssetManager::UpdateAssetFilepath(AssetHandle handle, const std::filesystem::path& filepath)
{
	if (!IsAssetHandleValid(handle))
		return false;

	const std::filesystem::path normalizedFilepath = NormalizeAssetPath(filepath);
	if (normalizedFilepath.empty())
		return false;

	m_AssetRegistry.Get(handle).m_Filepath = normalizedFilepath;
	bool result = false;
	SerializeAssetRegistry(&result);
	return result;
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
		SerializeAssetRegistry();
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
		SerializeAssetRegistry();
	return handles.size();
}

AssetHandle EditorAssetManager::GetHandleFromFilepath(const std::filesystem::path& filepath) const
{
	const std::filesystem::path normalizedFilepath = NormalizeAssetPath(filepath);
	if (normalizedFilepath.empty())
		return 0;

	AssetHandle handle = 0;
	m_AssetRegistry.ForeachChecked([&](const AssetRegistry::ValueType& value)
		{
			if (AssetPathsEqual(value.second.m_Filepath, normalizedFilepath))
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

const AssetRegistry& EditorAssetManager::GetAssetRegistry() const
{
	return m_AssetRegistry;
}

void EditorAssetManager::SerializeAssetRegistry(bool* result)
{
	if (!m_AssetRegistry.Serialize())
	{
		WHP_CORE_ERROR("[Asset Manager] Asset serializing failed!");
		if (result)
			*result = false;
	}
	else
	{
		if (result)
			*result = true;
	}
}

bool EditorAssetManager::DeserializeAssetRegistry()
{
	return m_AssetRegistry.Deserialize();
}

_WHIP_END
