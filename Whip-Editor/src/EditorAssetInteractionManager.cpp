#include <Whip-Editor/EditorAssetInteractionManager.h>

#include <Whip-Editor/EditorLayer.h>

#include <Whip/Asset/AssetMetadata.h>
#include <Whip/Asset/AssetUtils.h>

#include <algorithm>

_WHIP_START

namespace
{
	bool PathIsOrIsUnder(const std::filesystem::path& path, const std::filesystem::path& directory)
	{
		const std::filesystem::path normalizedPath = path.lexically_normal();
		const std::filesystem::path normalizedDirectory = directory.lexically_normal();
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

	std::filesystem::path MakeUniquePath(const std::filesystem::path& targetPath)
	{
		std::error_code error;
		if (!std::filesystem::exists(targetPath, error))
			return targetPath;

		const std::filesystem::path parent = targetPath.parent_path();
		const std::string stem = targetPath.stem().string();
		const std::string extension = targetPath.extension().string();
		for (uint32_t index = 1; index < 10000; ++index)
		{
			std::filesystem::path candidate = parent / std::format("{}_{}{}", stem, index, extension);
			error.clear();
			if (!std::filesystem::exists(candidate, error))
				return candidate;
		}

		return targetPath;
	}

	std::filesystem::path DefaultImportDirectoryForType(AssetType type)
	{
		switch (type)
		{
		case AssetType::Scene: return "Scenes";
		case AssetType::Texture2D: return "textures";
		case AssetType::Audio: return "Audios";
		case AssetType::Font: return "fonts";
		case AssetType::Animation: return "Animations";
		case AssetType::AnimationController: return "Animations";
		case AssetType::Entity: return "EntityTemplates";
		case AssetType::None: return {};
		}
		return {};
	}
}

bool EditorAssetInteractionManager::HandleViewportAssetDrop(EditorLayer& layer, AssetHandle handle, int32_t textureSpriteIndex) const
{
	return layer.HandleViewportAssetDrop(handle, textureSpriteIndex);
}

bool EditorAssetInteractionManager::HandleContentBrowserAssetOpen(EditorLayer& layer, AssetHandle handle) const
{
	return layer.HandleContentBrowserAssetOpen(handle);
}

bool EditorAssetInteractionManager::HandleContentBrowserAssetInspect(EditorLayer& layer, AssetHandle handle) const
{
	return layer.HandleContentBrowserAssetInspect(handle);
}

void EditorAssetInteractionManager::SetStartScene(AssetHandle handle) const
{
	Ref<Project> activeProject = Project::GetActive();
	if (!activeProject || handle == 0)
		return;

	if (!activeProject->GetEditorAssetManager()->IsAssetHandleValid(handle) ||
		activeProject->GetEditorAssetManager()->GetAssetType(handle) != AssetType::Scene)
	{
		return;
	}

	activeProject->GetConfig().m_StartScene = handle;
	Project::SaveActive();
	WHP_EDITOR_INFO(std::string("[Project] Start scene set: ") + activeProject->GetEditorAssetManager()->GetFilepath(handle).generic_string());
}

bool EditorAssetInteractionManager::CreateSpriteEntityFromTexture(EditorLayer& layer, AssetHandle handle, const glm::vec3& position, int32_t textureSpriteIndex) const
{
	return layer.CreateSpriteEntityFromTexture(handle, position, textureSpriteIndex);
}

AssetHandle EditorAssetInteractionManager::ImportExternalAssetFile(const std::filesystem::path& sourcePath) const
{
	Ref<Project> activeProject = Project::GetActive();
	if (!activeProject)
		return 0;

	std::error_code error;
	if (!std::filesystem::exists(sourcePath, error) || !std::filesystem::is_regular_file(sourcePath, error))
		return 0;

	const AssetType type = Utils::TryGetAssetTypeFromFileExtension(sourcePath.extension());
	if (type == AssetType::None)
	{
		WHP_EDITOR_WARN(std::string("[Asset Import] Unsupported dropped file: ") + sourcePath.string());
		return 0;
	}

	const std::filesystem::path assetDirectory = Project::GetActiveAssetDirectory();
	std::filesystem::path assetPath = sourcePath;
	if (!PathIsOrIsUnder(sourcePath, assetDirectory))
	{
		const std::filesystem::path importDirectory = assetDirectory / DefaultImportDirectoryForType(type);
		std::filesystem::create_directories(importDirectory, error);
		if (error)
		{
			WHP_EDITOR_WARN(std::string("[Asset Import] Could not create import directory: ") + error.message());
			return 0;
		}

		assetPath = MakeUniquePath(importDirectory / sourcePath.filename());
		error.clear();
		std::filesystem::copy_file(sourcePath, assetPath, std::filesystem::copy_options::none, error);
		if (error)
		{
			WHP_EDITOR_WARN(std::string("[Asset Import] Could not copy dropped file: ") + error.message());
			return 0;
		}
	}

	error.clear();
	std::filesystem::path relativePath = std::filesystem::relative(assetPath, assetDirectory, error).lexically_normal();
	if (error)
		return 0;

	if (AssetHandle existingHandle = activeProject->GetEditorAssetManager()->GetHandleFromFilepath(relativePath); existingHandle != 0)
		return existingHandle;

	return activeProject->GetEditorAssetManager()->ImportAsset(relativePath);
}

glm::vec3 EditorAssetInteractionManager::GetViewportMouseWorldPosition(const EditorLayer& layer) const
{
	return layer.GetViewportMouseWorldPosition();
}

_WHIP_END
