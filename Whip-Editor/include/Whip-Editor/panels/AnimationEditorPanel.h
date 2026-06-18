#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/UUID.h>
#include <Whip/Asset/Asset.h>
#include <Whip/Animation/Animation2D.h>
#include <Whip/Animation/AnimationController.h>
#include <Whip/Asset/AssetManager.h>
#include <Whip/Render/Texture.h>
#include <Whip-Editor/UI/UISettings.h>

#include <vector>
#include <functional>
#include <array>

#include <glm/vec2.hpp>

_WHIP_START

class AnimationEditorPanel
{
public:
	AnimationEditorPanel();
	~AnimationEditorPanel();

	void Open() { SetOpen(true); }
	void Close() { SetOpen(false); }
	void SetOpen(bool open);
	bool IsOpen() const { return m_Open; }
	bool ConsumeOpenDirty();

	void OnImGuiRender();
	void OnImGuiRenderEmbedded();
	bool OpenAsset(AssetHandle handle, bool openWindow = true);
	bool WantsShortcutCapture() const { return m_ShortcutContextActive; }
	bool ShouldConsumeShortcutAction(UI::EditorShortcutAction action) const;
	bool ExecuteShortcutAction(UI::EditorShortcutAction action);
	void HandleShortcutInput(const UI::UISettings& settings);

	//void LoadIcon(Icon iconType, Ref<Texture2D> iconTexture);
	void SetRefreshAssetTreeCallback(const std::function<void()>& func) { m_RefreshAssetTreeCallback = func; }
private:
	enum class AnimationEditorMode
	{
		Clip,
		Controller
	};

	enum class AnimationEditorClipboardType
	{
		None,
		Frame,
		ControllerState,
		ControllerTransition,
		ControllerBlueprintNode
	};

	enum class AnimationControllerWorkspaceTab
	{
		StateMachine,
		TransitionBlueprint
	};

	struct AnimationClipSnapshot
	{
		bool m_Valid = false;
		AssetHandle m_Handle = 0;
		std::string m_Name;
		bool m_Loop = false;
		std::vector<AnimationFrame> m_Frames;
		std::vector<AnimationEventKey> m_Events;
		std::vector<AnimationVec3Key> m_TranslationKeys;
		std::vector<AnimationVec3Key> m_RotationKeys;
		std::vector<AnimationVec3Key> m_ScaleKeys;
		std::vector<AnimationVec4Key> m_ColorKeys;
		int m_SelectedFrameIndex = -1;
	};

	struct AnimationControllerSnapshot
	{
		bool m_Valid = false;
		AssetHandle m_Handle = 0;
		std::string m_DefaultState;
		std::vector<AnimationControllerState> m_States;
		std::vector<AnimationControllerParameter> m_Parameters;
		std::vector<AnimationControllerTransition> m_AnyStateTransitions;
		glm::vec2 m_EntryGraphPosition{ 0.0f, 0.0f };
		glm::vec2 m_AnyStateGraphPosition{ 0.0f, 0.0f };
		glm::vec2 m_ExitGraphPosition{ 0.0f, 0.0f };
		int m_SelectedStateIndex = 0;
		int m_SelectedParameterIndex = -1;
		int m_SelectedTransitionSourceIndex = -1;
		int m_SelectedTransitionIndex = -1;
	};

	struct AnimationEditorSnapshot
	{
		AnimationEditorMode m_Mode = AnimationEditorMode::Clip;
		AnimationClipSnapshot m_Clip;
		AnimationControllerSnapshot m_Controller;
	};

	struct AnimationEditorClipboard
	{
		AnimationEditorClipboardType m_Type = AnimationEditorClipboardType::None;
		AnimationFrame m_Frame;
		AnimationControllerState m_State;
		AnimationControllerTransition m_Transition;
		AnimationControllerBlueprintNode m_BlueprintNode;
	};

	void DrawAnimationDragDropArea(float width, float height);
	void DrawControllerDragDropArea(float width, float height);
	void DrawPlaybackControls(float width, float height);
	void DrawNewButton(float width, float leftPadding);
	void DrawCloseButton(float width, float leftPadding);
	void DrawSaveButton(float width, float leftPadding);
	void DrawNameInput(float width, float leftPadding);
	void DrawAnimationSelector(float width, float leftPadding);
	void DrawControllerSelector(float width);
	void DrawTimeline(float width, float timelineHeight, float totalHeight);
	void DrawFrameList(float width);
	void DrawAddFrameButton(float width);
	void DrawRemoveFrameButton(float width);
	void DrawImportFramesButton(float width);
	void DrawPreviewPane(float width, float height);
	void DrawFrameEditor(float width);
	void DrawFrameBatchTools(float width);
	void DrawControllerEditor(float height);
	void DrawControllerParameters(float width, float height);
	void DrawControllerGraph(float width, float height);
	void DrawControllerStateInspector(float width, float height);
	void DrawControllerTransitionInspector(AnimationControllerTransition& transition, bool allowExitTarget);
	void DrawTransitionConditionGraph(AnimationControllerTransition& transition, float height);
	void DrawControllerValidation();
	void DrawEditorContent(bool showWindowControls);
	void DrawCompactSummary();
	void DrawWindowControls();
	void DrawMinimizedStrip();
	void CaptureWindowRect();
	void RequestFullscreen();
	void RestoreWindowRect();
	AnimationControllerTransition* GetSelectedControllerTransition();
	void ClearSelectedControllerTransition();
	void RemoveSelectedControllerTransition();
	void AutoLayoutControllerGraph();
	void SaveCurrentController();
	void ImportTextureFolderFrames();
	void NormalizeFrameDurations(float frameDuration);
	AnimationEditorSnapshot CaptureSnapshot() const;
	void RestoreSnapshot(const AnimationEditorSnapshot& snapshot);
	void PushHistory();
	bool Undo();
	bool Redo();
	bool CopySelection();
	bool CutSelection();
	bool PasteSelection();
	bool DuplicateSelection();
	bool DeleteSelection();
	bool DuplicateSelectedFrame();
	bool DeleteSelectedFrame();
	bool DuplicateSelectedControllerState();
	bool DeleteSelectedControllerState();
	void PasteFrame(const AnimationFrame& frame);
	void PasteControllerState(const AnimationControllerState& state);
	bool PasteControllerTransition(const AnimationControllerTransition& transition);
	std::string GetWindowTitle() const;
	void UpdatePreview();
	void StepPreview(int direction);
	void StopPreview(bool resetSelection);

	Ref<Animation2D> m_CurrentAnimation = nullptr;
	Ref<AnimationController> m_CurrentController = nullptr;
	AnimationEditorMode m_EditorMode = AnimationEditorMode::Clip;
	int m_SelectedFrameIndex = -1;
	int m_SelectedControllerStateIndex = 0;
	int m_SelectedControllerParameterIndex = -1;
	int m_SelectedTransitionSourceStateIndex = -1;
	int m_SelectedTransitionIndex = -1;
	int m_SelectedConditionNodeIndex = -1;
	uint32_t m_SelectedBlueprintNodeId = 0;
	uint32_t m_PendingBlueprintLinkNodeId = 0;
	uint32_t m_PendingBlueprintLinkPin = 0;
	bool m_PendingBlueprintLinkFromInput = false;
	uint32_t m_ContextBlueprintLinkNodeId = 0;
	uint32_t m_ContextBlueprintLinkPin = 0;
	bool m_ContextBlueprintLinkFromInput = false;
	int m_PendingTransitionSourceStateIndex = -1;
	float m_ControllerGraphZoom = 1.0f;
	float m_BlueprintGraphZoom = 1.0f;
	glm::vec2 m_ControllerGraphPan{ 28.0f, 28.0f };
	glm::vec2 m_BlueprintGraphPan{ 18.0f, 18.0f };
	glm::vec2 m_BlueprintContextSpawnPosition{ 0.0f, 0.0f };
	std::string m_BlueprintNodeSearch;
	AnimationControllerWorkspaceTab m_ControllerWorkspaceTab = AnimationControllerWorkspaceTab::StateMachine;
	bool m_ControllerWorkspaceTabSelectionRequested = false;
	bool m_BlueprintNodeSearchFocusRequested = false;
	bool m_ControllerGraphSnapToGrid = true;
	bool m_FrameControllerGraphRequested = false;
	bool m_ShowOnionSkin = true;
	bool m_PreviewPlaying = false;
	bool m_PreviewPaused = false;
	bool m_Minimized = false;
	bool m_Fullscreen = false;
	bool m_FullscreenRequested = false;
	bool m_FocusRequested = false;
	bool m_HasRestoreRect = false;
	glm::vec2 m_RestorePosition{ 120.0f, 90.0f };
	glm::vec2 m_RestoreSize{ 1120.0f, 680.0f };
	float m_PreviewElapsed = 0.0f;
	float m_DefaultFrameDuration = 0.1f;
	bool m_Open = false;
	bool m_OpenDirty = false;
	bool m_ShortcutContextActive = false;
	std::vector<AnimationEditorSnapshot> m_UndoStack;
	std::vector<AnimationEditorSnapshot> m_RedoStack;
	AnimationEditorClipboard m_Clipboard;

	std::function<void()> m_RefreshAssetTreeCallback;
};

_WHIP_END
