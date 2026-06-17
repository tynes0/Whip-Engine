#pragma once

#include <Whip.h>
#include <Whip/UI/UIProjectLoader.h>
#include <Whip/UI/UIProject.h>
#include <Whip/UI/UISettings.h>
#include <Whip/UI/UIStatistics.h>
#include <Whip/Render/EditorCamera.h>
#include <Whip/Audio/AudioEngine.h>

#include "panels/SceneHierarchyPanel.h"
#include "panels/ContentBrowserPanel.h"
#include "panels/AnimationEditorPanel.h"
#include "panels/ConsolePanel.h"

#include <FileWatch.h>

#include <chrono>
#include <mutex>
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
	virtual ~EditorLayer() = default;

	virtual void OnAttach() override;
	virtual void OnDetach() override;
	virtual void OnUpdate(Timestep ts) override;
	virtual void OnImGuiRender() override;
	virtual void OnEvent(Event& event) override;
private:
	struct SceneHistoryEntry;

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
	bool HandleViewportAssetDrop(AssetHandle handle);
	bool HandleContentBrowserAssetOpen(AssetHandle handle);
	bool CreateSpriteEntityFromTexture(AssetHandle handle, const glm::vec3& position);
	AssetHandle ImportExternalAssetFile(const std::filesystem::path& sourcePath);
	glm::vec3 GetViewportMouseWorldPosition() const;

	bool BuildProjectScripts();
	void ReloadAssembly(bool resetAppAssemblyFilepath = true);
	void StartScriptSourceWatcher();
	void StopScriptSourceWatcher();
	void HandleScriptSourceEvent(const std::string& path, filewatch::Event eventType);
	void ProcessScriptSourceChanges();
	void SetScriptBuildStatus(const std::string& message, bool warning = false, bool failure = false);
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
	struct ProjectHistoryEntry;
	ProjectHistoryEntry CaptureProjectHistory() const;
	void RestoreProjectHistory(const ProjectHistoryEntry& entry);
	bool ExecuteEditorAction(UI::EditorShortcutAction action);
	bool IsEditorActionAvailable(UI::EditorShortcutAction action) const;
	void OpenCommandPalette();
	void DrawCommandPalette();

	void UIToolbar();
private:
	enum class SceneState
	{
		Edit = 0,
		Play = 1,
		Simulate = 2
	};

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

	// UI's
	UI::UIProjectLoader m_ProjectLoader;
	UI::UIProject m_UIProject;
	UI::UISettings m_UISettings;
	UI::UIStatistics m_UIStatistics;
	UI::PopupHandler m_PopupHandler;
	std::vector<std::filesystem::path> m_RecentProjects;
	std::filesystem::path m_LastProjectPath;
	ContentBrowserPanel::Preferences m_ContentBrowserPreferences;
	bool m_HasContentBrowserPreferences = false;
	bool m_CommandPaletteOpen = false;
	bool m_CommandPaletteFocusSearch = false;
	char m_CommandPaletteFilter[128]{ 0 };

	Scope<filewatch::FileWatch<std::string>> m_ScriptSourceWatcher;
	std::filesystem::path m_ScriptSourceWatchDirectory;
	std::mutex m_ScriptSourceMutex;
	std::chrono::steady_clock::time_point m_LastScriptSourceChangeTime{};
	std::filesystem::path m_LastScriptSourceChangePath;
	std::string m_LastScriptSourceChangeEvent;
	bool m_ScriptSourceDirty = false;
	bool m_ScriptSourceQueuedWhileRunning = false;
	std::string m_ScriptBuildStatus = "Scripts idle";
	std::chrono::steady_clock::time_point m_ScriptBuildStatusTime{};
	bool m_ScriptBuildStatusWarning = false;
	bool m_ScriptBuildStatusFailure = false;

	struct ProjectHistoryEntry
	{
		bool m_Valid = false;
		ProjectConfig m_Config;
		std::filesystem::path m_ProjectPath;
		std::filesystem::path m_AssetRegistryPath;
		std::string m_ProjectFileContents;
		std::string m_AssetRegistryContents;
		std::unordered_map<std::string, std::string> m_SceneFileContents;
	};

	struct SceneHistoryEntry
	{
		Ref<Scene> m_SceneSnapshot;
		std::filesystem::path m_EditorScenePath;
		std::vector<UUID> m_SelectedEntities;
		ProjectHistoryEntry m_ProjectSnapshot;
	};

	// scene
	Ref<Scene> m_ActiveScene;
	Ref<Scene> m_EditorScene;
	std::filesystem::path m_EditorScenePath;
	std::vector<SceneHistoryEntry> m_UndoStack;
	std::vector<SceneHistoryEntry> m_RedoStack;
	std::vector<UUID> m_EntityClipboard;
	bool m_GizmoHistoryActive = false;
	bool m_SceneDirty = false;
	std::chrono::steady_clock::time_point m_LastSceneRecoverySnapshot{};

	// framebuffer
	Ref<Framebuffer> m_Framebuffer;

	// gizmo
	int m_GizmoType = -1;
	bool m_GizmoHovered = false;
	bool m_GizmoUsing = false;

	// states
	SceneState m_SceneState = SceneState::Edit;

	// panels
	SceneHierarchyPanel m_SceneHierarchyPanel;
	AnimationEditorPanel m_AnimationEditorPanel;
	Scope<ContentBrowserPanel> m_ContentBrowserPanel;

	Ref<AudioSource> m_AudioSource;
};

_WHIP_END
