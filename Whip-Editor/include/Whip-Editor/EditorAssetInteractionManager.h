#pragma once

#include <Whip.h>

#include <filesystem>

_WHIP_START

class EditorLayer;

class EditorAssetInteractionManager
{
public:
	explicit EditorAssetInteractionManager(EditorLayer* boundedLayer = nullptr);
	~EditorAssetInteractionManager();

	void Bind(EditorLayer& layer);

	bool HandleViewportAssetDrop(AssetHandle handle, int32_t textureSpriteIndex = -1) const;
	bool HandleContentBrowserAssetOpen(AssetHandle handle) const;
	bool HandleContentBrowserAssetInspect(AssetHandle handle) const;
	void SetStartScene(AssetHandle handle) const;
	bool CreateSpriteEntityFromTexture(AssetHandle handle, const glm::vec3& position, int32_t textureSpriteIndex = -1) const;
	AssetHandle ImportExternalAssetFile(const std::filesystem::path& sourcePath) const;
	glm::vec3 GetViewportMouseWorldPosition() const;

private:
	EditorLayer& GetLayer() const;

	EditorLayer* m_BoundedLayer = nullptr;
};

_WHIP_END
