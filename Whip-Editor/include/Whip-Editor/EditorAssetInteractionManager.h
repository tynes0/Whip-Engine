#pragma once

#include <Whip.h>

#include <filesystem>

_WHIP_START

class EditorLayer;

class EditorAssetInteractionManager
{
public:
	bool HandleViewportAssetDrop(EditorLayer& layer, AssetHandle handle, int32_t textureSpriteIndex = -1) const;
	bool HandleContentBrowserAssetOpen(EditorLayer& layer, AssetHandle handle) const;
	bool HandleContentBrowserAssetInspect(EditorLayer& layer, AssetHandle handle) const;
	void SetStartScene(AssetHandle handle) const;
	bool CreateSpriteEntityFromTexture(EditorLayer& layer, AssetHandle handle, const glm::vec3& position, int32_t textureSpriteIndex = -1) const;
	AssetHandle ImportExternalAssetFile(const std::filesystem::path& sourcePath) const;
	glm::vec3 GetViewportMouseWorldPosition(const EditorLayer& layer) const;
};

_WHIP_END
