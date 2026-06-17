#pragma once

#include <Whip.h>

#include <functional>
#include <vector>

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

private:
	struct AssetEditorDocument
	{
		AssetHandle m_Handle = 0;
		bool m_Open = true;
		bool m_FocusRequested = true;
		bool m_Compact = false;
		bool m_FullscreenRequested = false;
	};

	void HandleEditorTabShortcut();
	void FocusNextEditor();
	void DrawDocument(AssetEditorDocument& document);
	void DrawToolbar(AssetEditorDocument& document, const AssetMetadata& metadata);
	void DrawMetadata(AssetHandle handle, const AssetMetadata& metadata) const;
	void DrawTextureInspector(AssetHandle handle, const AssetMetadata& metadata, bool compact) const;
	void DrawAudioInspector(AssetHandle handle, bool compact) const;
	void DrawFontInspector(AssetHandle handle, const AssetMetadata& metadata, bool compact) const;
	void DrawSceneInspector(AssetHandle handle, bool compact) const;
	void DrawAnimationInspector(AssetHandle handle, const AssetMetadata& metadata, bool compact) const;
	void DrawEntityInspector(AssetHandle handle, const AssetMetadata& metadata, bool compact) const;

	std::string MakeWindowTitle(AssetHandle handle, const AssetMetadata& metadata) const;

	std::vector<AssetEditorDocument> m_Documents;
	AssetHandle m_LastFocusedEditor = 0;
	bool m_OpenDirty = false;

	std::function<void(AssetHandle)> m_OpenSceneCallback;
	std::function<void(AssetHandle)> m_SetStartSceneCallback;
	std::function<bool(AssetHandle)> m_OpenAnimationCallback;
};

_WHIP_END
