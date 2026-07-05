#pragma once

#include <Whip.h>
#include <Whip-Editor/UI/UIProjectLoader.h>
#include <Whip-Editor/Panels/ContentBrowserPanel.h>

#include <chrono>
#include <filesystem>
#include <memory>
#include <vector>

#include "EditorManagerBase.h"

_WHIP_START

class EditorProjectManager : public EditorManagerBase // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	explicit EditorProjectManager(EditorLayer* boundedLayer = nullptr);
	~EditorProjectManager() override;

	UI::UIProjectLoader& GetLoader();
	const UI::UIProjectLoader& GetLoader() const;

	const std::vector<std::filesystem::path>& GetRecentProjects() const;

	void SetupProjectLoader();
	void LoadRecentProjects();
	void SaveRecentProjects() const;
	void AddRecentProject(const std::filesystem::path& path);
	bool ForgetRecentProject(const std::filesystem::path& path);
	bool DeleteRecentProject(const std::filesystem::path& path);

	static bool ShouldIncludeRecentProject(const std::filesystem::path& path);
	static std::filesystem::path GetRecentProjectsPath();
	static std::filesystem::path GetPreferencesPath();

	void LoadEditorPreferences();
	void SaveEditorPreferences() const;
	void ApplyPreferencesToContentBrowser();

	bool NewProject(const UI::ProjectCreateSettings& settings);
	void SaveProject() const;
	void FinishProjectSettings();
	void MigrateProjectNativeFileExtensions() const;
	bool OpenProject();
	bool OpenProject(const std::filesystem::path& path);
	void ResetEditorProjectState();
	void UpdateAsyncOperations();
	void DrawAsyncProgressOverlay();
	void CancelAsyncOperations(bool waitForCompletion = false);
	bool IsProjectOperationRunning() const;

private:
	enum class ProjectOpenStage : uint8_t
	{
		Idle = 0,
		PreparingProject,
		LoadingScene
	};

	struct ProjectOpenResult
	{
		std::filesystem::path m_ProjectPath;
		Ref<Project> m_Project;
		Ref<Scene> m_LoadedScene;
		std::filesystem::path m_LoadedScenePath;
		AssetHandle m_ConfiguredStartScene = 0;
		AssetHandle m_StartScene = 0;
		bool m_ScriptBuildSucceeded = true;
		std::string m_Error;
	};

	bool BeginOpenProjectAsync(const std::filesystem::path& projectPath);
	void BeginStartSceneLoadAsync();
	void CompletePreparedProjectOpen();
	void CompleteStartSceneLoad();
	void FinishProjectOpen(bool success, std::string status);
	void ConfigureLoadedProjectPanels();

	UI::UIProjectLoader m_ProjectLoader;
	std::vector<std::filesystem::path> m_RecentProjects;
	std::filesystem::path m_LastProjectPath;
	ContentBrowserPanel::Preferences m_ContentBrowserPreferences;
	bool m_HasContentBrowserPreferences = false;
	Async::JobHandle m_ProjectOpenJob;
	std::shared_ptr<ProjectOpenResult> m_ProjectOpenResult;
	std::chrono::steady_clock::time_point m_ProjectOpenStartedAt{};
	ProjectOpenStage m_ProjectOpenStage = ProjectOpenStage::Idle;
};

_WHIP_END
