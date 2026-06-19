#pragma once

#include <Whip.h>
#include <Whip-Editor/UI/UIProjectLoader.h>
#include <Whip-Editor/Panels/ContentBrowserPanel.h>

#include <filesystem>
#include <vector>

_WHIP_START

class EditorLayer;

class EditorProjectManager
{
public:
	explicit EditorProjectManager(EditorLayer* boundedLayer = nullptr);
	~EditorProjectManager();

	UI::UIProjectLoader& GetLoader() { return m_ProjectLoader; }
	const UI::UIProjectLoader& GetLoader() const { return m_ProjectLoader; }

	const std::vector<std::filesystem::path>& GetRecentProjects() const { return m_RecentProjects; }

	void Bind(EditorLayer& layer);

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

	bool NewProject(const UI::ProjectCreateSettings& settings);
	void SaveProject() const;
	void FinishProjectSettings();
	void MigrateProjectNativeFileExtensions() const;
	bool OpenProject();
	bool OpenProject(const std::filesystem::path& path);
	void ResetEditorProjectState();

private:
	EditorLayer& GetLayer() const;

	EditorLayer* m_BoundedLayer = nullptr;
	UI::UIProjectLoader m_ProjectLoader;
	std::vector<std::filesystem::path> m_RecentProjects;
	std::filesystem::path m_LastProjectPath;
	ContentBrowserPanel::Preferences m_ContentBrowserPreferences;
	bool m_HasContentBrowserPreferences = false;
};

_WHIP_END
