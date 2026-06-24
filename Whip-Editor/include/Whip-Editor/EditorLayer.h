#pragma once

#include <Whip.h>

#include <Whip/Render/EditorCamera.h>

#include <Whip-Editor/Managers/EditorAssetInteractionManager.h>
#include <Whip-Editor/Managers/EditorHistoryManager.h>
#include <Whip-Editor/Managers/EditorScriptManager.h>
#include <Whip-Editor/Managers/EditorProjectManager.h>
#include <Whip-Editor/Managers/EditorSceneManager.h>
#include <Whip-Editor/Managers/EditorPanelManager.h>
#include <Whip-Editor/Managers/EditorEntityTemplateManager.h>
#include <Whip-Editor/Managers/EditorShortcutManager.h>

#include <Whip-Editor/Panels/SceneHierarchyPanel.h>
#include <Whip-Editor/Panels/ContentBrowserPanel.h>
#include <Whip-Editor/Panels/AnimationEditorPanel.h>
#include <Whip-Editor/Panels/AssetEditorPanel.h>
#include <Whip-Editor/Panels/AssistantPanel.h>
#include <Whip-Editor/Panels/ProjectHealthPanel.h>

#include <Whip-Editor/UI/UIProject.h>
#include <Whip-Editor/UI/UISettings.h>
#include <Whip-Editor/UI/UIStatistics.h>

_WHIP_START
	class EditorLayer : public Layer // NOLINT(cppcoreguidelines-special-member-functions)
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
	friend class EditorEntityTemplateManager;
	friend class EditorHistoryManager;
	friend class EditorPanelManager;
	friend class EditorProjectManager;
	friend class EditorSceneManager;
	friend class EditorScriptManager;
	friend class EditorShortcutManager;

	bool OnKeyPressed(KeyPressedEvent& event);
	bool OnMouseButtonPressed(MouseButtonPressedEvent& event);
	bool OnWindowDrop(WindowDropEvent& event);

	void DrawEditorGrid();
	void OnOverlayRender();

	bool HasProjectLoaded() const;

	bool ExecuteEditorAction(UI::EditorShortcutAction action);
	bool IsEditorActionAvailable(UI::EditorShortcutAction action) const;
	void RegisterEditorShortcuts();
	void RebuildEditorPanelRegistry();
	void DrawEditorShellTitlebar(bool projectLoaded);
	void DrawEditorMenuBar(bool projectLoaded);
	void OpenCommandPalette();
	void DrawCommandPalette();
	Assistant::ContextSnapshot BuildAssistantContextSnapshot() const;
	bool ApplyAssistantProposal(const Assistant::ToolProposal& proposal);

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

	// Managers
	EditorAssetInteractionManager m_AssetInteractionManager;
	EditorEntityTemplateManager m_EntityTemplateManager;
	EditorHistoryManager m_HistoryManager;
	EditorProjectManager m_ProjectManager;
	EditorScriptManager m_ScriptManager;
	EditorSceneManager m_SceneManager;
	EditorPanelManager m_PanelManager;
	EditorShortcutManager m_ShortcutManager;

	// UI's
	UI::UIProject m_UIProject;
	UI::UISettings m_UISettings;
	UI::UIStatistics m_UIStatistics;
	UI::PopupHandler m_PopupHandler;
	bool m_CommandPaletteOpen = false;
	bool m_CommandPaletteFocusSearch = false;
	bool m_CommandPaletteAvailableOnly = false;
	int m_CommandPaletteScopeFilter = -1;
	char m_CommandPaletteFilter[128]{ 0 };

	// framebuffer
	Ref<Framebuffer> m_Framebuffer;

	// gizmo
	int m_GizmoType = -1;
	bool m_GizmoHovered = false;
	bool m_GizmoUsing = false;

	// panels
	Scope<CallbackEditorPanel> m_StatisticsPanelAdapter;
	Scope<CallbackEditorPanel> m_ConsolePanelAdapter;
	SceneHierarchyPanel m_SceneHierarchyPanel;
	AnimationEditorPanel m_AnimationEditorPanel;
	AssetEditorPanel m_AssetEditorPanel;
	AssistantPanel m_AssistantPanel;
	ProjectHealthPanel m_ProjectHealthPanel;
	Scope<ContentBrowserPanel> m_ContentBrowserPanel;
};

_WHIP_END
