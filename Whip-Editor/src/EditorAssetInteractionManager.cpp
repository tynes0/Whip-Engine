#include <Whip-Editor/EditorAssetInteractionManager.h>

#include <Whip-Editor/EditorLayer.h>

#include <Whip/Asset/AssetManager.h>
#include <Whip/Asset/AssetMetadata.h>
#include <Whip/Asset/AssetUtils.h>

#include <algorithm>
#include <imgui.h>

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
	if (handle == 0 || !layer.HasProjectLoaded())
		return false;

	Ref<Project> activeProject = Project::GetActive();
	if (!activeProject->GetEditorAssetManager()->IsAssetHandleValid(handle))
		return false;

	const AssetType type = activeProject->GetEditorAssetManager()->GetAssetType(handle);
	switch (type)
	{
	case AssetType::Scene:
		layer.OpenScene(handle);
		return true;
	case AssetType::Entity:
		return layer.InstantiateEntityTemplate(handle);
	case AssetType::Texture2D:
		return CreateSpriteEntityFromTexture(layer, handle, GetViewportMouseWorldPosition(layer), textureSpriteIndex);
	default:
		WHP_EDITOR_WARN("[Viewport] This Asset type cannot be dropped into the viewport yet.");
		return false;
	}
}

bool EditorAssetInteractionManager::HandleContentBrowserAssetOpen(EditorLayer& layer, AssetHandle handle) const
{
	if (handle == 0 || !layer.HasProjectLoaded())
		return false;

	Ref<Project> activeProject = Project::GetActive();
	if (!activeProject->GetEditorAssetManager()->IsAssetHandleValid(handle))
		return false;

	switch (activeProject->GetEditorAssetManager()->GetAssetType(handle))
	{
	case AssetType::Scene:
		layer.OpenScene(handle);
		return true;
	case AssetType::Entity:
		return layer.InstantiateEntityTemplate(handle);
	default:
		return false;
	}
}

bool EditorAssetInteractionManager::HandleContentBrowserAssetInspect(EditorLayer& layer, AssetHandle handle) const
{
	if (handle == 0 || !layer.HasProjectLoaded())
		return false;

	Ref<Project> activeProject = Project::GetActive();
	if (!activeProject->GetEditorAssetManager()->IsAssetHandleValid(handle))
		return false;

	layer.m_AssetEditorPanel.OpenAsset(handle);
	return true;
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
	if (!layer.HasProjectLoaded() || !layer.m_EditorScene || layer.m_SceneState != EditorSceneState::Edit)
		return false;

	Ref<Project> activeProject = Project::GetActive();
	if (!activeProject->GetEditorAssetManager()->IsAssetHandleValid(handle) ||
		activeProject->GetEditorAssetManager()->GetAssetType(handle) != AssetType::Texture2D)
	{
		return false;
	}

	layer.m_HistoryManager.CaptureSceneHistory(layer);
	const auto& metadata = activeProject->GetEditorAssetManager()->GetMetadata(handle);
	const auto& sprites = metadata.m_TextureSettings.m_Sprites;
	const bool validSpriteIndex = textureSpriteIndex >= 0 && textureSpriteIndex < static_cast<int32_t>(sprites.size());
	const TextureSpriteRect* spriteRect = validSpriteIndex ? &sprites[static_cast<size_t>(textureSpriteIndex)] : nullptr;
	const std::string name = spriteRect ? spriteRect->m_Name : (metadata.m_Filepath.stem().empty() ? "Sprite" : metadata.m_Filepath.stem().string());
	Entity sprite = layer.m_EditorScene->CreateEntity(name);
	auto& transform = sprite.GetComponent<TransformComponent>();
	transform.m_Translation = position;

	auto texture = AssetManager::GetAsset<Texture2D>(handle);
	if (texture && texture->IsLoaded())
	{
		const float pixelsPerUnit = metadata.m_TextureSettings.m_PixelsPerUnit > 0.0f ? metadata.m_TextureSettings.m_PixelsPerUnit : 100.0f;
		const float spriteWidth = spriteRect ? static_cast<float>(spriteRect->m_Width) : static_cast<float>(texture->GetWidth());
		const float spriteHeight = spriteRect ? static_cast<float>(spriteRect->m_Height) : static_cast<float>(texture->GetHeight());
		transform.m_Scale = {
			glm::max(spriteWidth / pixelsPerUnit, 0.1f),
			glm::max(spriteHeight / pixelsPerUnit, 0.1f),
			1.0f
		};
	}

	auto& spriteRenderer = sprite.AddComponent<SpriteRendererComponent>();
	spriteRenderer.m_Texture = handle;
	spriteRenderer.m_TextureSpriteIndex = validSpriteIndex ? textureSpriteIndex : -1;
	spriteRenderer.m_Color = glm::vec4(1.0f);
	layer.m_SceneHierarchyPanel.SetSelectedEntity(sprite);
	WHP_EDITOR_INFO(std::string("[Viewport] Created sprite entity from texture ") + metadata.m_Filepath.generic_string());
	return true;
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
	const glm::vec2 viewportSize = layer.m_ViewportBounds[1] - layer.m_ViewportBounds[0];
	const glm::vec3 fallback = layer.m_EditorCamera.GetPosition() + layer.m_EditorCamera.GetForwardDirection() * glm::max(layer.m_EditorCamera.GetDistance(), 1.0f);
	if (viewportSize.x <= 0.0f || viewportSize.y <= 0.0f)
		return { fallback.x, fallback.y, 0.0f };

	const ImVec2 mouse = ImGui::GetMousePos();
	const float x = glm::clamp((mouse.x - layer.m_ViewportBounds[0].x) / viewportSize.x, 0.0f, 1.0f);
	const float y = glm::clamp((mouse.y - layer.m_ViewportBounds[0].y) / viewportSize.y, 0.0f, 1.0f);
	const glm::vec2 ndc{ x * 2.0f - 1.0f, (1.0f - y) * 2.0f - 1.0f };

	const glm::mat4 inverseViewProjection = glm::inverse(layer.m_EditorCamera.GetViewProjection());
	glm::vec4 nearPoint = inverseViewProjection * glm::vec4(ndc.x, ndc.y, -1.0f, 1.0f);
	glm::vec4 farPoint = inverseViewProjection * glm::vec4(ndc.x, ndc.y, 1.0f, 1.0f);
	nearPoint /= nearPoint.w;
	farPoint /= farPoint.w;

	const glm::vec3 rayOrigin = glm::vec3(nearPoint);
	const glm::vec3 rayDirection = glm::normalize(glm::vec3(farPoint - nearPoint));
	if (glm::abs(rayDirection.z) < 0.0001f)
		return { fallback.x, fallback.y, 0.0f };

	const float t = -rayOrigin.z / rayDirection.z;
	const glm::vec3 worldPosition = rayOrigin + rayDirection * t;
	return { worldPosition.x, worldPosition.y, 0.0f };
}

_WHIP_END
