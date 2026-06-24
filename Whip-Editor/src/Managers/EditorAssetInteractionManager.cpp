#include <Whip-Editor/Managers/EditorAssetInteractionManager.h>

#include <Whip-Editor/Managers/EditorSceneManager.h>
#include <Whip-Editor/Managers/EditorHistoryManager.h>
#include <Whip-Editor/EditorLayer.h>

#include <Whip/Asset/AssetManager.h>
#include <Whip/Asset/AssetMetadata.h>
#include <Whip/Asset/AssetUtils.h>

#include <imgui.h>

#include <cmath>
#include <utility>

#include "Whip-Editor/Helpers/Utils.h"

_WHIP_START
	namespace
{
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
		case AssetType::Animation:
		case AssetType::AnimationController: return "Animations";
		case AssetType::Entity: return "EntityTemplates";
		case AssetType::None: return {};
		}
		return {};
	}

	glm::vec2 EstimateSpriteWorldSize(AssetHandle handle, int32_t textureSpriteIndex)
	{
		Ref<Project> activeProject = Project::GetActive();
		if (!activeProject || !activeProject->GetEditorAssetManager() || !activeProject->GetEditorAssetManager()->IsAssetHandleValid(handle))
			return { 1.0f, 1.0f };

		const AssetMetadata& metadata = activeProject->GetEditorAssetManager()->GetMetadata(handle);
		const auto& sprites = metadata.m_TextureSettings.m_Sprites;
		const bool validSpriteIndex = textureSpriteIndex >= 0 && std::cmp_less(textureSpriteIndex, sprites.size());
		const TextureSpriteRect* spriteRect = validSpriteIndex ? &sprites[static_cast<size_t>(textureSpriteIndex)] : nullptr;
		Ref<Texture2D> texture = AssetManager::GetAsset<Texture2D>(handle);
		const float pixelsPerUnit = metadata.m_TextureSettings.m_PixelsPerUnit > 0.0f ? metadata.m_TextureSettings.m_PixelsPerUnit : 100.0f;
		const float width = spriteRect ? static_cast<float>(spriteRect->m_Width) : (texture && texture->IsLoaded() ? static_cast<float>(texture->GetWidth()) : pixelsPerUnit);
		const float height = spriteRect ? static_cast<float>(spriteRect->m_Height) : (texture && texture->IsLoaded() ? static_cast<float>(texture->GetHeight()) : pixelsPerUnit);
		return {
			glm::max(width / pixelsPerUnit, 0.1f),
			glm::max(height / pixelsPerUnit, 0.1f)
		};
	}
}

EditorAssetInteractionManager::EditorAssetInteractionManager(EditorLayer* boundedLayer)
	: EditorManagerBase(boundedLayer)
{
}

EditorAssetInteractionManager::~EditorAssetInteractionManager() = default;

bool EditorAssetInteractionManager::HandleViewportAssetDrop(AssetHandle handle, int32_t textureSpriteIndex) const
{
	EditorLayer& layer = GetLayer();
	if (handle == 0 || !layer.HasProjectLoaded())
		return false;

	Ref<Project> activeProject = Project::GetActive();
	if (!activeProject->GetEditorAssetManager()->IsAssetHandleValid(handle))
		return false;

	switch (const AssetType type = activeProject->GetEditorAssetManager()->GetAssetType(handle); type)
	{
	case AssetType::Scene:
		layer.m_SceneManager.OpenScene(handle);
		return true;
	case AssetType::Entity:
		return layer.m_EntityTemplateManager.InstantiateEntityTemplate(handle);
	case AssetType::Texture2D:
		return CreateSpriteEntityFromTexture(handle, GetViewportMouseWorldPosition(), textureSpriteIndex);
	case AssetType::Audio:
	case AssetType::Font:
	case AssetType::Animation:
	case AssetType::AnimationController:
	case AssetType::None:
		WHP_EDITOR_WARN("[Viewport] This Asset type cannot be dropped into the viewport yet.");
		return false;
	}
	return false;
}

bool EditorAssetInteractionManager::HandleViewportAssetDrops(const std::vector<std::pair<AssetHandle, int32_t>>& assetReferences) const
{
	EditorLayer& layer = GetLayer();
	if (assetReferences.empty() || !layer.HasProjectLoaded() || !layer.m_SceneManager.EditorScene() || layer.m_SceneManager.State() != EditorSceneState::Edit)
		return false;

	std::vector<std::pair<AssetHandle, int32_t>> textureReferences;
	textureReferences.reserve(assetReferences.size());
	Ref<Project> activeProject = Project::GetActive();
	for (const auto& [handle, spriteIndex] : assetReferences)
	{
		if (handle == 0 || !activeProject->GetEditorAssetManager()->IsAssetHandleValid(handle))
			continue;

		if (activeProject->GetEditorAssetManager()->GetAssetType(handle) == AssetType::Texture2D)
			textureReferences.emplace_back(handle, spriteIndex);
		else if (assetReferences.size() == 1)
			return HandleViewportAssetDrop(handle, spriteIndex);
	}

	if (textureReferences.empty())
		return false;

	const size_t count = textureReferences.size();
	const uint32_t columns = static_cast<uint32_t>(std::ceil(std::sqrt(static_cast<float>(count))));
	const glm::vec3 origin = GetViewportMouseWorldPosition();
	glm::vec2 maxSpriteSize{ 0.1f, 0.1f };
	for (const auto& [handle, spriteIndex] : textureReferences)
		maxSpriteSize = glm::max(maxSpriteSize, EstimateSpriteWorldSize(handle, spriteIndex));

	const glm::vec2 spacing = maxSpriteSize + glm::vec2(0.20f);
	const glm::vec3 topLeft = origin - glm::vec3(spacing.x * static_cast<float>(std::min<size_t>(columns, count) - 1) * 0.5f, 0.0f, 0.0f);
	layer.m_HistoryManager.CaptureSceneHistory();

	bool createdAny = false;
	for (size_t index = 0; index < textureReferences.size(); ++index)
	{
		const uint32_t column = static_cast<uint32_t>(index % columns);
		const uint32_t row = static_cast<uint32_t>(index / columns);
		const glm::vec3 position = topLeft + glm::vec3(static_cast<float>(column) * spacing.x, -static_cast<float>(row) * spacing.y, 0.0f);
		createdAny = CreateSpriteEntityFromTexture(textureReferences[index].first, position, textureReferences[index].second, false) || createdAny;
	}

	if (createdAny)
		WHP_EDITOR_INFO("[Viewport] Created {0} sprite entities from multi-asset drop.", textureReferences.size());
	return createdAny;
}

bool EditorAssetInteractionManager::HandleContentBrowserAssetOpen(AssetHandle handle) const
{
	EditorLayer& layer = GetLayer();
	if (handle == 0 || !layer.HasProjectLoaded())
		return false;

	Ref<Project> activeProject = Project::GetActive();
	if (!activeProject->GetEditorAssetManager()->IsAssetHandleValid(handle))
		return false;

	switch (activeProject->GetEditorAssetManager()->GetAssetType(handle))
	{
	case AssetType::Scene:
		layer.m_SceneManager.OpenScene(handle);
		return true;
	case AssetType::Entity:
		return layer.m_EntityTemplateManager.InstantiateEntityTemplate(handle);
	case AssetType::Texture2D:
	case AssetType::Audio:
	case AssetType::Font:
	case AssetType::Animation:
	case AssetType::AnimationController:
	case AssetType::None:
		return false;
	}
	return false;
}

bool EditorAssetInteractionManager::HandleContentBrowserAssetInspect(AssetHandle handle) const
{
	EditorLayer& layer = GetLayer();
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

bool EditorAssetInteractionManager::CreateSpriteEntityFromTexture(AssetHandle handle, const glm::vec3& position, int32_t textureSpriteIndex, bool captureHistory) const
{
	EditorLayer& layer = GetLayer();
	if (!layer.HasProjectLoaded() || !layer.m_SceneManager.EditorScene() || layer.m_SceneManager.State() != EditorSceneState::Edit)
		return false;

	Ref<Project> activeProject = Project::GetActive();
	if (!activeProject->GetEditorAssetManager()->IsAssetHandleValid(handle) ||
		activeProject->GetEditorAssetManager()->GetAssetType(handle) != AssetType::Texture2D)
	{
		return false;
	}

	if (captureHistory)
		layer.m_HistoryManager.CaptureSceneHistory();
	const auto& metadata = activeProject->GetEditorAssetManager()->GetMetadata(handle);
	const auto& sprites = metadata.m_TextureSettings.m_Sprites;
	const bool validSpriteIndex = textureSpriteIndex >= 0 && std::cmp_less(textureSpriteIndex, sprites.size());
	const TextureSpriteRect* spriteRect = validSpriteIndex ? &sprites[static_cast<size_t>(textureSpriteIndex)] : nullptr;
	const std::string name = spriteRect ? spriteRect->m_Name : (metadata.m_Filepath.stem().empty() ? "Sprite" : metadata.m_Filepath.stem().string());
	Entity sprite = layer.m_SceneManager.EditorScene()->CreateEntity(name);
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
	if (!EditorUtils::PathIsOrIsUnder(sourcePath, assetDirectory))
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

glm::vec3 EditorAssetInteractionManager::GetViewportMouseWorldPosition() const
{
	const EditorLayer& layer = GetLayer();
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
