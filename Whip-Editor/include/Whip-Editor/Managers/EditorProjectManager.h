#pragma once

#include <Whip.h>
#include <Whip-Editor/UI/UIProjectLoader.h>
#include <Whip-Editor/Panels/ContentBrowserPanel.h>

#include <filesystem>
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

private:
	UI::UIProjectLoader m_ProjectLoader;
	std::vector<std::filesystem::path> m_RecentProjects;
	std::filesystem::path m_LastProjectPath;
	ContentBrowserPanel::Preferences m_ContentBrowserPreferences;
	bool m_HasContentBrowserPreferences = false;
};

_WHIP_END
