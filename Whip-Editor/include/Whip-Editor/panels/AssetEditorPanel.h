#pragma once

#include <Whip-Editor/panels/EditorPanel.h>

#include <Whip.h>

#include <functional>
#include <array>
#include <string>
#include <vector>

#include <glm/vec2.hpp>

_WHIP_START

class AssetEditorPanel : public EditorPanel
{
public:
	AssetEditorPanel();

	void OpenAsset(AssetHandle handle);
	void CloseAll();
	void OnImGuiRender() override;
	void SetOpen(bool open) override;
	bool IsOpen() const override { return m_Open; }
	bool CanOpenFromMenu() const override { return HasOpenEditors(); }
	bool HasOpenEditors() const;
	bool ConsumeOpenDirty() override;

	void SetOpenSceneCallback(std::function<void(AssetHandle)> callback) { m_OpenSceneCallback = std::move(callback); }
	void SetSetStartSceneCallback(std::function<void(AssetHandle)> callback) { m_SetStartSceneCallback = std::move(callback); }
	void SetOpenAnimationCallback(std::function<bool(AssetHandle)> callback) { m_OpenAnimationCallback = std::move(callback); }
	void SetDrawAnimationEditorCallback(std::function<void()> callback) { m_DrawAnimationEditorCallback = std::move(callback); }
	void SetRefreshAssetTreeCallback(std::function<void()> callback) { m_RefreshAssetTreeCallback = std::move(callback); }

	enum class TextureEditorTool
	{
		Brush = 0,
		Eraser,
		Picker,
		Fill,
		Slice
	};

private:
	struct TextureEditorState
	{
		AssetHandle m_LoadedHandle = 0;
		uint32_t m_Width = 0;
		uint32_t m_Height = 0;
		uint32_t m_Channels = 4;
		ImageFormat m_Format = ImageFormat::None;
		std::vector<uint8_t> m_Pixels;
		TextureEditorTool m_Tool = TextureEditorTool::Brush;
		std::array<float, 4> m_BrushColor = { 1.0f, 1.0f, 1.0f, 1.0f };
		int m_BrushSize = 1;
		float m_Zoom = 8.0f;
		glm::vec2 m_Pan = { 0.0f, 0.0f };
		bool m_ShowGrid = true;
		bool m_LiveApply = true;
		bool m_Dirty = false;
		std::string m_Status;
		std::vector<std::vector<uint8_t>> m_UndoPixels;
		std::vector<std::vector<uint8_t>> m_RedoPixels;
		bool m_PaintStrokeActive = false;
		int m_SliceCellWidth = 32;
		int m_SliceCellHeight = 32;
		int m_SlicePadding = 0;
		int m_SliceSpacing = 0;
		int m_AutoSliceMinPixels = 24;
		int m_AutoSliceBackgroundTolerance = 24;
		int m_AutoSliceMergeGap = 0;
		int m_AutoSlicePadding = 1;
		int m_AutoSliceExtrudePixels = 0;
		bool m_AutoSliceSeparateDiagonalTouches = true;
		bool m_AutoSliceExportPngs = false;
		int m_SelectedSpriteIndex = -1;
		bool m_IsSlicing = false;
		glm::vec2 m_SliceStart = { 0.0f, 0.0f };
		glm::vec2 m_SliceEnd = { 0.0f, 0.0f };
	};

	struct FontEditorState
	{
		std::string m_PreviewText = "Whip Engine\nAsset Editor Preview";
		float m_PreviewScale = 1.0f;
		bool m_ShowAtlasGrid = false;
		Ref<Texture2D> m_PreviewTexture;
		AssetHandle m_PreviewTextureFont = 0;
		std::string m_PreviewTextureText;
		float m_PreviewTextureScale = 0.0f;
		uint32_t m_PreviewTextureWidth = 0;
		uint32_t m_PreviewTextureHeight = 0;
	};

	struct AssetEditorDocument
	{
		AssetHandle m_Handle = 0;
		bool m_Open = true;
		bool m_FocusRequested = true;
		TextureEditorState m_TextureState;
		FontEditorState m_FontState;
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
	void RestoreWorkspaceRect(bool anchorToMouse = false);
	void DrawMetadata(AssetHandle handle, const AssetMetadata& metadata) const;
	void DrawTextureInspector(AssetEditorDocument& document, const AssetMetadata& metadata, bool compact);
	void DrawAudioInspector(AssetHandle handle, bool compact) const;
	void DrawFontInspector(AssetEditorDocument& document, const AssetMetadata& metadata, bool compact) const;
	void DrawSceneInspector(AssetHandle handle, bool compact) const;
	void DrawAnimationInspector(AssetHandle handle, const AssetMetadata& metadata, bool compact);
	void DrawEntityInspector(AssetHandle handle, const AssetMetadata& metadata, bool compact) const;
	bool EnsureTextureEditorState(TextureEditorState& state, AssetHandle handle, const Ref<Texture2D>& texture);
	void ReloadTextureEditorState(TextureEditorState& state, AssetHandle handle, const Ref<Texture2D>& texture);
	void ApplyTextureEditorState(TextureEditorState& state, const Ref<Texture2D>& texture);
	bool SaveTextureEditorState(TextureEditorState& state, const AssetMetadata& metadata);
	bool SaveAssetMetadata(AssetHandle handle, const AssetMetadata& metadata);
	void HandleTextureEditorShortcuts(TextureEditorState& state, const Ref<Texture2D>& texture);
	void PushTextureUndo(TextureEditorState& state);
	bool UndoTextureEdit(TextureEditorState& state, const Ref<Texture2D>& texture);
	bool RedoTextureEdit(TextureEditorState& state, const Ref<Texture2D>& texture);
	std::array<uint8_t, 4> ReadTexturePixel(const TextureEditorState& state, int x, int y) const;
	bool WriteTexturePixel(TextureEditorState& state, int x, int y, const std::array<uint8_t, 4>& color);
	bool PaintTextureBrush(TextureEditorState& state, int x, int y, bool erase);
	bool FillTextureRegion(TextureEditorState& state, int x, int y, const std::array<uint8_t, 4>& color);

	std::string MakeTabLabel(AssetHandle handle, const AssetMetadata& metadata) const;

	std::vector<AssetEditorDocument> m_Documents;
	AssetHandle m_ActiveDocument = 0;
	AssetHandle m_EmbeddedAnimationHandle = 0;
	bool m_Minimized = false;
	bool m_Fullscreen = false;
	bool m_FullscreenRequested = false;
	bool m_FocusRequested = false;
	bool m_HasRestoreRect = false;
	glm::vec2 m_RestorePosition{ 120.0f, 90.0f };
	glm::vec2 m_RestoreSize{ 1040.0f, 640.0f };

	std::function<void(AssetHandle)> m_OpenSceneCallback;
	std::function<void(AssetHandle)> m_SetStartSceneCallback;
	std::function<bool(AssetHandle)> m_OpenAnimationCallback;
	std::function<void()> m_DrawAnimationEditorCallback;
	std::function<void()> m_RefreshAssetTreeCallback;
};

_WHIP_END
