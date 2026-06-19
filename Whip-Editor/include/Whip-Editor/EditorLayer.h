#pragma once

#include <Whip.h>
#include <Whip-Editor/EditorAssetInteractionManager.h>
#include <Whip-Editor/EditorHistoryManager.h>
#include <Whip-Editor/EditorScriptManager.h>
#include <Whip-Editor/EditorProjectManager.h>
#include <Whip-Editor/EditorSceneManager.h>
#include <Whip-Editor/UI/UIProject.h>
#include <Whip-Editor/UI/UISettings.h>
#include <Whip-Editor/UI/UIStatistics.h>
#include <Whip/Render/EditorCamera.h>
#include <Whip/Audio/AudioEngine.h>

#include <Whip-Editor/Panels/SceneHierarchyPanel.h>
#include <Whip-Editor/Panels/ContentBrowserPanel.h>
#include <Whip-Editor/Panels/AnimationEditorPanel.h>
#include <Whip-Editor/Panels/AssetEditorPanel.h>
#include <Whip-Editor/Panels/ConsolePanel.h>
#include <Whip-Editor/Panels/EditorPanelManager.h>

_WHIP_START

class EditorLayer : public Layer
{
public:
	EditorLayer();
	~EditorLayer() override = default;

	void OnAttach() override;
	void OnDetach() override;
	void OnUpdate(Timestep ts) override;
	void OnImGuiRender() override;
	void OnEvent(Event& event) override;
private:
	using SceneState = EditorSceneState;

	friend class EditorAssetInteractionManager;
	friend class EditorHistoryManager;
	friend class EditorProjectManager;
	friend class EditorSceneManager;

	bool OnKeyPressed(KeyPressedEvent& event);
	bool OnMouseButtonPressed(MouseButtonPressedEvent& event);
	bool OnWindowDrop(WindowDropEvent& event);

	void DrawEditorGrid();
	void OnOverlayRender();

	bool HasProjectLoaded() const;

	void SaveEntityTemplate(Entity entityIn);
	void ApplyEntityTemplate(Entity entityIn);
	void RevertEntityTemplate(Entity entityIn);
	void UnpackEntityTemplate(Entity entityIn);
	Entity FindPrefabRoot(Entity entityIn) const;
	void RemovePrefabLinksRecursive(Entity entityIn);
	bool InstantiateEntityTemplate(AssetHandle handle);

	bool ExecuteEditorAction(UI::EditorShortcutAction action);
	bool IsEditorActionAvailable(UI::EditorShortcutAction action) const;
	void RebuildEditorPanelRegistry();
	void DrawEditorShellTitlebar(bool projectLoaded);
	void DrawEditorMenuBar(bool projectLoaded);
	void OpenCommandPalette();
	void DrawCommandPalette();

	void UIToolbar();
	Timestep m_Ts;

	// camera
	EditorCamera m_EditorCamera;

	// viewport
	glm::vec2 m_ViewportBounds[2]{};
	glm::vec2 m_ViewportSize = { 1.0f, 1.0f };
	bool m_ViewportHovered = false;
	bool m_ViewportFocused = false;

	// entity
	Entity m_HoveredEntity;
	Entity m_LastSelectedEntity;

	EditorAssetInteractionManager m_AssetInteractionManager;
	EditorHistoryManager m_HistoryManager;
	EditorProjectManager m_ProjectManager;
	EditorScriptManager m_ScriptManager;
	EditorSceneManager m_SceneManager;

	// UI's
	UI::UIProject m_UIProject;
	UI::UISettings m_UISettings;
	UI::UIStatistics m_UIStatistics;
	UI::PopupHandler m_PopupHandler;
	bool m_CommandPaletteOpen = false;
	bool m_CommandPaletteFocusSearch = false;
	char m_CommandPaletteFilter[128]{ 0 };

	// framebuffer
	Ref<Framebuffer> m_Framebuffer;

	// gizmo
	int m_GizmoType = -1;
	bool m_GizmoHovered = false;
	bool m_GizmoUsing = false;

	// panels
	EditorPanelManager m_PanelManager;
	Scope<CallbackEditorPanel> m_StatisticsPanelAdapter;
	Scope<CallbackEditorPanel> m_ConsolePanelAdapter;
	SceneHierarchyPanel m_SceneHierarchyPanel;
	AnimationEditorPanel m_AnimationEditorPanel;
	AssetEditorPanel m_AssetEditorPanel;
	Scope<ContentBrowserPanel> m_ContentBrowserPanel;

	Ref<AudioSource> m_AudioSource;
};

_WHIP_END
