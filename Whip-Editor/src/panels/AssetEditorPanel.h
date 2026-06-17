#pragma once

#include <Whip.h>

#include <functional>
#include <vector>

#include <glm/vec2.hpp>

_WHIP_START

class AssetEditorPanel
{
public:
	void OpenAsset(AssetHandle handle);
	void CloseAll();
	void OnImGuiRender();
	bool HasOpenEditors() const;
	bool ConsumeOpenDirty();

	void SetOpenSceneCallback(std::function<void(AssetHandle)> callback) { m_OpenSceneCallback = std::move(callback); }
	void SetSetStartSceneCallback(std::function<void(AssetHandle)> callback) { m_SetStartSceneCallback = std::move(callback); }
	void SetOpenAnimationCallback(std::function<bool(AssetHandle)> callback) { m_OpenAnimationCallback = std::move(callback); }
	void SetDrawAnimationEditorCallback(std::function<void()> callback) { m_DrawAnimationEditorCallback = std::move(callback); }

private:
	struct AssetEditorDocument
	{
		AssetHandle m_Handle = 0;
		bool m_Open = true;
		bool m_FocusRequested = true;
	};

	void HandleWorkspaceTabShortcut();
	void FocusNextEditor();
	void DrawMinimizedStrip();
	void DrawWorkspaceHeader();
	void DrawWorkspaceTabs();
	void DrawDocumentContent(AssetEditorDocument& document);
	void DrawDocumentToolbar(AssetHandle handle, const AssetMetadata& metadata) const;
	void CaptureWorkspaceRect();
	void RequestFullscreen();
	void RestoreWorkspaceRect();
	void DrawMetadata(AssetHandle handle, const AssetMetadata& metadata) const;
	void DrawTextureInspector(AssetHandle handle, const AssetMetadata& metadata, bool compact) const;
	void DrawAudioInspector(AssetHandle handle, bool compact) const;
	void DrawFontInspector(AssetHandle handle, const AssetMetadata& metadata, bool compact) const;
	void DrawSceneInspector(AssetHandle handle, bool compact) const;
	void DrawAnimationInspector(AssetHandle handle, const AssetMetadata& metadata, bool compact);
	void DrawEntityInspector(AssetHandle handle, const AssetMetadata& metadata, bool compact) const;

	std::string MakeTabLabel(AssetHandle handle, const AssetMetadata& metadata) const;

	std::vector<AssetEditorDocument> m_Documents;
	AssetHandle m_ActiveDocument = 0;
	AssetHandle m_EmbeddedAnimationHandle = 0;
	bool m_Open = false;
	bool m_Minimized = false;
	bool m_Fullscreen = false;
	bool m_FullscreenRequested = false;
	bool m_FocusRequested = false;
	bool m_HasRestoreRect = false;
	glm::vec2 m_RestorePosition{ 120.0f, 90.0f };
	glm::vec2 m_RestoreSize{ 1040.0f, 640.0f };
	bool m_OpenDirty = false;

	std::function<void(AssetHandle)> m_OpenSceneCallback;
	std::function<void(AssetHandle)> m_SetStartSceneCallback;
	std::function<bool(AssetHandle)> m_OpenAnimationCallback;
	std::function<void()> m_DrawAnimationEditorCallback;
};

_WHIP_END
