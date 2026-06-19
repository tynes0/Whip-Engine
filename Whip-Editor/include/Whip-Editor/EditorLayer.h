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

#include <Whip-Editor/panels/SceneHierarchyPanel.h>
#include <Whip-Editor/panels/ContentBrowserPanel.h>
#include <Whip-Editor/panels/AnimationEditorPanel.h>
#include <Whip-Editor/panels/AssetEditorPanel.h>
#include <Whip-Editor/panels/ConsolePanel.h>
#include <Whip-Editor/panels/EditorPanelManager.h>

#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>

// TODOLIST
// - entity Asset
// - Spawn and destroy entity -> cs
// - add and destroy component (runtime) -> actually I don't think this is necessary
// - field arrays.
// - scene hierarchy -> update properties panel -> all of them will be table
// - scene settings
// - serialize runtime
// - add new Project popup
// - all the Project settings
// - fix font Asset
// - fix ContentBrowserPanel Asset tree
// - symmetric ContentBrowserPanel settings
// - fix animation editor drag drop size
// - there is an issue with SceneHierarchyPanel::draw_component (i guess...)
// - texture manager -> g_icons with this
// - AudioSource destroy

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
	using ProjectHistoryEntry = EditorHistoryManager::ProjectHistoryEntry;
	using SceneHistoryEntry = EditorHistoryManager::SceneHistoryEntry;

	friend class EditorAssetInteractionManager;
	friend class EditorHistoryManager;
	friend class EditorProjectManager;
	friend class EditorSceneManager;

	bool OnKeyPressed(KeyPressedEvent& event);
	bool OnMouseButtonPressed(MouseButtonPressedEvent& event);
	bool OnWindowDrop(WindowDropEvent& event);

	void DrawEditorGrid();
	void OnOverlayRender();

	bool NewProject(const UI::ProjectCreateSettings& settings);
	void SaveProject();
	void FinishProjectSettings();
	void MigrateProjectNativeFileExtensions();

	bool OpenProject();
	bool OpenProject(const std::filesystem::path& path);
	void ResetEditorProjectState();
	bool HasProjectLoaded() const;
	void SetupProjectLoader();
	void LoadRecentProjects();
	void SaveRecentProjects() const;
	void AddRecentProject(const std::filesystem::path& path);
	bool ForgetRecentProject(const std::filesystem::path& path);
	bool DeleteRecentProject(const std::filesystem::path& path);
	bool ShouldIncludeRecentProject(const std::filesystem::path& path) const;
	std::filesystem::path GetRecentProjectsPath() const;
	std::filesystem::path GetPreferencesPath() const;
	void LoadEditorPreferences();
	void SaveEditorPreferences() const;
	void ApplyPreferencesToContentBrowser();

	void NewScene();
	void OpenScene(AssetHandle handle);
	void CloseScene();
	void SaveScene();
	void SaveSceneAs();
	void MarkSceneDirty();
	void MarkSceneClean();
	void WriteSceneRecoverySnapshot(const char* reason);
	std::filesystem::path GetSceneRecoveryPath() const;
	void SaveEntityTemplate(Entity entityIn);
	void ApplyEntityTemplate(Entity entityIn);
	void RevertEntityTemplate(Entity entityIn);
	void UnpackEntityTemplate(Entity entityIn);
	Entity FindPrefabRoot(Entity entityIn) const;
	void RemovePrefabLinksRecursive(Entity entityIn);
	bool InstantiateEntityTemplate(AssetHandle handle);
	bool HandleViewportAssetDrop(AssetHandle handle, int32_t textureSpriteIndex = -1);
	bool HandleContentBrowserAssetOpen(AssetHandle handle);
	bool HandleContentBrowserAssetInspect(AssetHandle handle);
	void SetStartScene(AssetHandle handle);
	bool CreateSpriteEntityFromTexture(AssetHandle handle, const glm::vec3& position, int32_t textureSpriteIndex = -1);
	AssetHandle ImportExternalAssetFile(const std::filesystem::path& sourcePath);
	glm::vec3 GetViewportMouseWorldPosition() const;

	void ProcessRuntimeSceneTransition();
	bool LoadRuntimeScene(AssetHandle handle);
	bool UnloadRuntimeScene();
	void StopActiveRuntimeSceneForTransition();
	void StartActiveRuntimeSceneForTransition(AssetHandle handle);

	void SerializeScene(Ref<Scene> sceneIn, const std::filesystem::path& path);

	void OnScenePlay();
	void OnSceneSimulate();
	void OnSceneStop();
	void OnScenePause();

	void OnDuplicatedEntity();
	void OnDeletedEntity();
	void OnSelectAllEntities();
	void OnCopyEntities();
	void OnPasteEntities();
	void OnCutEntities();
	void UndoScene();
	void RedoScene();
	void CaptureSceneHistory(bool includeProjectSnapshot = false);
	void RestoreSceneHistory(const SceneHistoryEntry& entry);
	void ClearSceneHistory();
	ProjectHistoryEntry CaptureProjectHistory() const;
	void RestoreProjectHistory(const ProjectHistoryEntry& entry);
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
	UI::UIProjectLoader& m_ProjectLoader;
	UI::UIProject m_UIProject;
	UI::UISettings m_UISettings;
	UI::UIStatistics m_UIStatistics;
	UI::PopupHandler m_PopupHandler;
	std::vector<std::filesystem::path>& m_RecentProjects;
	std::filesystem::path& m_LastProjectPath;
	ContentBrowserPanel::Preferences& m_ContentBrowserPreferences;
	bool& m_HasContentBrowserPreferences;
	bool m_CommandPaletteOpen = false;
	bool m_CommandPaletteFocusSearch = false;
	char m_CommandPaletteFilter[128]{ 0 };

	// scene
	Ref<Scene>& m_ActiveScene;
	Ref<Scene>& m_EditorScene;
	std::filesystem::path& m_EditorScenePath;
	std::vector<SceneHistoryEntry>& m_UndoStack;
	std::vector<SceneHistoryEntry>& m_RedoStack;
	std::vector<UUID>& m_EntityClipboard;
	bool& m_GizmoHistoryActive;
	bool& m_SceneDirty;
	std::chrono::steady_clock::time_point& m_LastSceneRecoverySnapshot;

	// framebuffer
	Ref<Framebuffer> m_Framebuffer;

	// gizmo
	int m_GizmoType = -1;
	bool m_GizmoHovered = false;
	bool m_GizmoUsing = false;

	// states
	SceneState& m_SceneState;

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
