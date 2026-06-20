#pragma once

#include <Whip.h>

#include <filesystem>

#include "EditorManagerBase.h"

_WHIP_START

class EditorAssetInteractionManager : public EditorManagerBase  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	explicit EditorAssetInteractionManager(EditorLayer* boundedLayer = nullptr);
	~EditorAssetInteractionManager() override;

	bool HandleViewportAssetDrop(AssetHandle handle, int32_t textureSpriteIndex = -1) const;
	bool HandleContentBrowserAssetOpen(AssetHandle handle) const;
	bool HandleContentBrowserAssetInspect(AssetHandle handle) const;
	void SetStartScene(AssetHandle handle) const;
	bool CreateSpriteEntityFromTexture(AssetHandle handle, const glm::vec3& position, int32_t textureSpriteIndex = -1) const;
	AssetHandle ImportExternalAssetFile(const std::filesystem::path& sourcePath) const;
	glm::vec3 GetViewportMouseWorldPosition() const;
};

_WHIP_END
