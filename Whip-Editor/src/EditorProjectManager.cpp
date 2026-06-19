#include <Whip-Editor/EditorProjectManager.h>

#include <Whip-Editor/EditorLayer.h>

#include <Whip/Asset/AssetMetadata.h>
#include <Whip/Utils/FileExtensions.h>

#include <fstream>

_WHIP_START

void EditorProjectManager::SetupProjectLoader(EditorLayer& layer) { layer.SetupProjectLoader(); }
void EditorProjectManager::LoadRecentProjects(EditorLayer& layer) { layer.LoadRecentProjects(); }
void EditorProjectManager::SaveRecentProjects() const
{
	std::ofstream stream(GetRecentProjectsPath(), std::ios::trunc);
	if (!stream)
		return;

	for (const auto& projectPath : m_RecentProjects)
		stream << projectPath.string() << '\n';
}
void EditorProjectManager::AddRecentProject(EditorLayer& layer, const std::filesystem::path& path) { layer.AddRecentProject(path); }
bool EditorProjectManager::ForgetRecentProject(EditorLayer& layer, const std::filesystem::path& path) { return layer.ForgetRecentProject(path); }
bool EditorProjectManager::DeleteRecentProject(EditorLayer& layer, const std::filesystem::path& path) { return layer.DeleteRecentProject(path); }
bool EditorProjectManager::ShouldIncludeRecentProject(const std::filesystem::path& path) const
{
	std::error_code error;
	std::filesystem::path normalizedPath = std::filesystem::weakly_canonical(path, error);
	if (error)
		normalizedPath = path;

	error.clear();
	std::filesystem::path workingDirectory = std::filesystem::weakly_canonical(std::filesystem::current_path(), error);
	if (error)
		workingDirectory = std::filesystem::current_path();

	error.clear();
	std::filesystem::path relativePath = std::filesystem::relative(normalizedPath, workingDirectory, error);
	if (!error && !relativePath.empty())
	{
		const std::filesystem::path firstComponent = *relativePath.begin();
		if (firstComponent != ".." && firstComponent != ".")
			return false;
	}

	return FileExtensions::IsProjectExtension(path);
}
std::filesystem::path EditorProjectManager::GetRecentProjectsPath() const { return std::filesystem::current_path() / "WhipHubRecentProjects.txt"; }
std::filesystem::path EditorProjectManager::GetPreferencesPath() const { return std::filesystem::current_path() / "WhipEditorPreferences.yaml"; }
void EditorProjectManager::LoadEditorPreferences(EditorLayer& layer) { layer.LoadEditorPreferences(); }
void EditorProjectManager::SaveEditorPreferences(const EditorLayer& layer) const { layer.SaveEditorPreferences(); }
void EditorProjectManager::ApplyPreferencesToContentBrowser(EditorLayer& layer) { layer.ApplyPreferencesToContentBrowser(); }
bool EditorProjectManager::NewProject(EditorLayer& layer, const UI::ProjectCreateSettings& settings) { return layer.NewProject(settings); }
void EditorProjectManager::SaveProject() const { Project::SaveActive(); }
void EditorProjectManager::FinishProjectSettings(EditorLayer& layer) { layer.FinishProjectSettings(); }
void EditorProjectManager::MigrateProjectNativeFileExtensions() const
{
	Ref<Project> activeProject = Project::GetActive();
	if (!activeProject || !activeProject->GetEditorAssetManager())
		return;

	std::vector<std::pair<AssetHandle, std::filesystem::path>> scenePaths;
	const auto& scenes = activeProject->GetEditorAssetManager()->GetAssetRegistry().GetFiltered(AssetType::Scene);
	scenePaths.reserve(scenes.size());
	for (const auto& [handle, metadata] : scenes)
	{
		if (FileExtensions::IsLegacySceneExtension(metadata.m_Filepath))
			scenePaths.emplace_back(handle, metadata.m_Filepath);
	}

	size_t migratedCount = 0;
	const std::filesystem::path assetDirectory = Project::GetActiveAssetDirectory();
	for (const auto& [handle, legacyRelativePath] : scenePaths)
	{
		std::filesystem::path modernRelativePath = legacyRelativePath;
		modernRelativePath.replace_extension(FileExtensions::Scene);

		const std::filesystem::path legacyPath = assetDirectory / legacyRelativePath;
		const std::filesystem::path modernPath = assetDirectory / modernRelativePath;

		std::error_code error;
		const bool modernExists = std::filesystem::exists(modernPath, error);
		error.clear();
		const bool legacyExists = std::filesystem::exists(legacyPath, error);
		if (!modernExists && legacyExists)
		{
			error.clear();
			std::filesystem::rename(legacyPath, modernPath, error);
			if (error)
				continue;
		}
		else if (!modernExists)
		{
			continue;
		}

		if (activeProject->GetEditorAssetManager()->UpdateAssetFilepath(handle, modernRelativePath))
			++migratedCount;
	}

	if (migratedCount > 0)
		WHP_EDITOR_INFO(std::string("[Project] Migrated ") + std::to_string(migratedCount) + " scene file extension(s) to .wscene.");
}
bool EditorProjectManager::OpenProject(EditorLayer& layer) { return layer.OpenProject(); }
bool EditorProjectManager::OpenProject(EditorLayer& layer, const std::filesystem::path& path) { return layer.OpenProject(path); }
void EditorProjectManager::ResetEditorProjectState(EditorLayer& layer) { layer.ResetEditorProjectState(); }

_WHIP_END
