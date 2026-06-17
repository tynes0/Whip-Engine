#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/UUID.h>
#include <Whip/Asset/Asset.h>
#include <Whip/Animation/Animation2D.h>
#include <Whip/Animation/AnimationController.h>
#include <Whip/Asset/AssetManager.h>
#include <Whip/Render/Texture.h>

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

	//void LoadIcon(Icon iconType, Ref<Texture2D> iconTexture);
	void SetRefreshAssetTreeCallback(const std::function<void()>& func) { m_RefreshAssetTreeCallback = func; }
private:
	void DrawAnimationDragDropArea(float width, float height);
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
	void DrawPreviewPane(float width, float height);
	void DrawFrameEditor(float width);
	void DrawControllerEditor(float height);
	void DrawControllerParameters(float width, float height);
	void DrawControllerGraph(float width, float height);
	void DrawControllerStateInspector(float width, float height);
	void DrawControllerTransitionInspector(AnimationControllerTransition& transition, bool allowExitTarget);
	AnimationControllerTransition* GetSelectedControllerTransition();
	void ClearSelectedControllerTransition();
	void RemoveSelectedControllerTransition();
	void AutoLayoutControllerGraph();
	void SaveCurrentController();
	void UpdatePreview();
	void StepPreview(int direction);
	void StopPreview(bool resetSelection);

	Ref<Animation2D> m_CurrentAnimation = nullptr;
	Ref<AnimationController> m_CurrentController = nullptr;
	int m_SelectedFrameIndex = -1;
	int m_SelectedControllerStateIndex = 0;
	int m_SelectedControllerParameterIndex = -1;
	int m_SelectedTransitionSourceStateIndex = -1;
	int m_SelectedTransitionIndex = -1;
	int m_PendingTransitionSourceStateIndex = -1;
	float m_ControllerGraphZoom = 1.0f;
	glm::vec2 m_ControllerGraphPan{ 28.0f, 28.0f };
	bool m_FrameControllerGraphRequested = false;
	bool m_PreviewPlaying = false;
	bool m_PreviewPaused = false;
	float m_PreviewElapsed = 0.0f;
	bool m_Open = true;
	bool m_OpenDirty = false;

	std::function<void()> m_RefreshAssetTreeCallback;
};

_WHIP_END
