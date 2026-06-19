#pragma once

#include <Whip.h>
#include <Whip-Editor/UI/UIProjectLoader.h>
#include <Whip-Editor/panels/ContentBrowserPanel.h>

#include <filesystem>
#include <vector>

_WHIP_START

class EditorLayer;

class EditorProjectManager
{
public:
	UI::UIProjectLoader& GetLoader() { return m_ProjectLoader; }
	const UI::UIProjectLoader& GetLoader() const { return m_ProjectLoader; }

	std::vector<std::filesystem::path>& GetRecentProjectsStorage() { return m_RecentProjects; }
	std::filesystem::path& GetLastProjectPathStorage() { return m_LastProjectPath; }
	ContentBrowserPanel::Preferences& GetContentBrowserPreferencesStorage() { return m_ContentBrowserPreferences; }
	bool& GetHasContentBrowserPreferencesStorage() { return m_HasContentBrowserPreferences; }

	void SetupProjectLoader(EditorLayer& layer);
	void LoadRecentProjects(EditorLayer& layer);
	void SaveRecentProjects() const;
	void AddRecentProject(EditorLayer& layer, const std::filesystem::path& path);
	bool ForgetRecentProject(EditorLayer& layer, const std::filesystem::path& path);
	bool DeleteRecentProject(EditorLayer& layer, const std::filesystem::path& path);
	bool ShouldIncludeRecentProject(const std::filesystem::path& path) const;

	std::filesystem::path GetRecentProjectsPath() const;
	std::filesystem::path GetPreferencesPath() const;
	void LoadEditorPreferences(EditorLayer& layer);
	void SaveEditorPreferences(const EditorLayer& layer) const;
	void ApplyPreferencesToContentBrowser(EditorLayer& layer);

	bool NewProject(EditorLayer& layer, const UI::ProjectCreateSettings& settings);
	void SaveProject() const;
	void FinishProjectSettings(EditorLayer& layer);
	void MigrateProjectNativeFileExtensions() const;
	bool OpenProject(EditorLayer& layer);
	bool OpenProject(EditorLayer& layer, const std::filesystem::path& path);
	void ResetEditorProjectState(EditorLayer& layer);

private:
	UI::UIProjectLoader m_ProjectLoader;
	std::vector<std::filesystem::path> m_RecentProjects;
	std::filesystem::path m_LastProjectPath;
	ContentBrowserPanel::Preferences m_ContentBrowserPreferences;
	bool m_HasContentBrowserPreferences = false;
};

_WHIP_END
