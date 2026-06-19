#include <Whip-Editor/EditorProjectManager.h>

#include <Whip-Editor/EditorLayer.h>
#include <Whip-Editor/EditorScriptManager.h>
#include <Whip-Editor/panels/ConsolePanel.h>

#include <Whip/Asset/AssetMetadata.h>
#include <Whip/Utils/FileExtensions.h>

#include <algorithm>
#include <fstream>
#include <yaml-cpp/yaml.h>

_WHIP_START

namespace
{
	void WriteVec3(YAML::Emitter& out, const glm::vec3& value)
	{
		out << YAML::Flow << YAML::BeginSeq << value.x << value.y << value.z << YAML::EndSeq;
	}

	glm::vec3 ReadVec3(const YAML::Node& node, const glm::vec3& fallback)
	{
		if (!node || !node.IsSequence() || node.size() != 3)
			return fallback;
		return { node[0].as<float>(fallback.x), node[1].as<float>(fallback.y), node[2].as<float>(fallback.z) };
	}

	UI::EditorTheme ThemeFromString(const std::string& value)
	{
		if (value == "Graphite")
			return UI::EditorTheme::Graphite;
		if (value == "Ember")
			return UI::EditorTheme::Ember;
		if (value == "Moss")
			return UI::EditorTheme::Moss;
		if (value == "Porcelain" || value == "Light")
			return UI::EditorTheme::Light;
		return UI::EditorTheme::WhipDark;
	}

	std::filesystem::path NormalizeProjectListPath(const std::filesystem::path& path)
	{
		if (path.empty())
			return {};

		std::error_code error;
		std::filesystem::path normalizedPath = std::filesystem::weakly_canonical(path, error);
		if (error)
		{
			error.clear();
			normalizedPath = std::filesystem::absolute(path, error);
		}

		return error ? path : normalizedPath.lexically_normal();
	}

	bool PathsMatchForRecentProject(const std::filesystem::path& left, const std::filesystem::path& right)
	{
		const std::filesystem::path normalizedLeft = NormalizeProjectListPath(left);
		const std::filesystem::path normalizedRight = NormalizeProjectListPath(right);
		return !normalizedLeft.empty() && normalizedLeft == normalizedRight;
	}

	bool PathIsOrIsUnder(const std::filesystem::path& path, const std::filesystem::path& directory)
	{
		const std::filesystem::path normalizedPath = path.lexically_normal();
		const std::filesystem::path normalizedDirectory = directory.lexically_normal();
		if (normalizedPath == normalizedDirectory)
			return true;

		auto pathIt = normalizedPath.begin();
		auto directoryIt = normalizedDirectory.begin();
		for (; directoryIt != normalizedDirectory.end(); ++directoryIt, ++pathIt)
		{
			if (pathIt == normalizedPath.end() || *pathIt != *directoryIt)
				return false;
		}

		return true;
	}
}

void EditorProjectManager::SetupProjectLoader(EditorLayer& layer)
{
	m_ProjectLoader.SetCreateProjectCallback([&layer](const UI::ProjectCreateSettings& settings) { return layer.NewProject(settings); });
	m_ProjectLoader.SetLoadProjectCallback([&layer]() { return layer.OpenProject(); });
	m_ProjectLoader.SetOpenRecentProjectCallback([this, &layer](const std::filesystem::path& path) {
		std::error_code error;
		if (!std::filesystem::exists(path, error))
		{
			WHP_EDITOR_WARN("[Whip Hub] Recent Project no longer exists.");
			m_ProjectLoader.SetStatus("Recent project no longer exists.");
			LoadRecentProjects(layer);
			return false;
		}

		const bool opened = layer.OpenProject(path);
		if (!opened)
			m_ProjectLoader.SetStatus("Project could not be opened.");
		return opened;
	});
	m_ProjectLoader.SetForgetRecentProjectCallback([this, &layer](const std::filesystem::path& path) {
		return ForgetRecentProject(layer, path);
	});
	m_ProjectLoader.SetDeleteRecentProjectCallback([this, &layer](const std::filesystem::path& path) {
		return DeleteRecentProject(layer, path);
	});
}

void EditorProjectManager::LoadRecentProjects(EditorLayer& layer)
{
	m_RecentProjects.clear();
	bool shouldRewrite = false;

	std::ifstream stream(GetRecentProjectsPath());
	if (!stream)
	{
		m_ProjectLoader.SetRecentProjects(m_RecentProjects);
		return;
	}

	std::string line;
	while (std::getline(stream, line))
	{
		if (line.empty())
			continue;

		std::filesystem::path path(line);
		std::error_code error;
		if (!std::filesystem::exists(path, error) || !FileExtensions::IsProjectExtension(path))
		{
			shouldRewrite = true;
			continue;
		}

		path = std::filesystem::weakly_canonical(path, error);
		if (error)
			path = std::filesystem::absolute(line, error);
		if (!ShouldIncludeRecentProject(path))
		{
			shouldRewrite = true;
			continue;
		}

		if (std::find(m_RecentProjects.begin(), m_RecentProjects.end(), path) == m_RecentProjects.end())
			m_RecentProjects.push_back(path);
		else
			shouldRewrite = true;

		if (m_RecentProjects.size() >= 10)
		{
			shouldRewrite = true;
			break;
		}
	}

	stream.close();
	if (shouldRewrite)
		SaveRecentProjects();
	m_ProjectLoader.SetRecentProjects(m_RecentProjects);
}

void EditorProjectManager::SaveRecentProjects() const
{
	std::ofstream stream(GetRecentProjectsPath(), std::ios::trunc);
	if (!stream)
		return;

	for (const auto& projectPath : m_RecentProjects)
		stream << projectPath.string() << '\n';
}

void EditorProjectManager::AddRecentProject(EditorLayer& layer, const std::filesystem::path& path)
{
	if (path.empty())
		return;

	std::error_code error;
	std::filesystem::path normalizedPath = std::filesystem::weakly_canonical(path, error);
	if (error)
	{
		error.clear();
		normalizedPath = std::filesystem::absolute(path, error);
	}
	if (error)
		normalizedPath = path;
	if (!ShouldIncludeRecentProject(normalizedPath))
		return;

	m_RecentProjects.erase(
		std::remove(m_RecentProjects.begin(), m_RecentProjects.end(), normalizedPath),
		m_RecentProjects.end());

	m_RecentProjects.insert(m_RecentProjects.begin(), normalizedPath);
	if (m_RecentProjects.size() > 10)
		m_RecentProjects.resize(10);

	m_LastProjectPath = normalizedPath;
	SaveRecentProjects();
	SaveEditorPreferences(layer);
	m_ProjectLoader.SetRecentProjects(m_RecentProjects);
}

bool EditorProjectManager::ForgetRecentProject(EditorLayer& layer, const std::filesystem::path& path)
{
	if (path.empty())
		return false;

	const size_t previousSize = m_RecentProjects.size();
	m_RecentProjects.erase(
		std::remove_if(m_RecentProjects.begin(), m_RecentProjects.end(),
			[&path](const std::filesystem::path& recentPath)
			{
				return PathsMatchForRecentProject(recentPath, path);
			}),
		m_RecentProjects.end());

	if (m_RecentProjects.size() == previousSize)
		return false;

	if (PathsMatchForRecentProject(m_LastProjectPath, path))
		m_LastProjectPath.clear();

	SaveRecentProjects();
	SaveEditorPreferences(layer);
	m_ProjectLoader.SetRecentProjects(m_RecentProjects);
	return true;
}

bool EditorProjectManager::DeleteRecentProject(EditorLayer& layer, const std::filesystem::path& path)
{
	if (path.empty() || !FileExtensions::IsProjectExtension(path))
		return false;

	std::error_code error;
	std::filesystem::path projectPath = NormalizeProjectListPath(path);
	const bool projectFileExists = std::filesystem::exists(projectPath, error);
	if (error)
		return false;
	if (!projectFileExists)
		return ForgetRecentProject(layer, path);

	if (!std::filesystem::is_regular_file(projectPath, error) || error)
		return false;

	const std::filesystem::path projectDirectory = NormalizeProjectListPath(projectPath.parent_path());
	if (projectDirectory.empty() || projectDirectory == projectDirectory.root_path())
		return false;

	if (projectDirectory.filename() != projectPath.stem())
	{
		WHP_EDITOR_WARN(std::string("[Whip Hub] Refusing to delete Project folder because it does not match the Project file name: ") + projectDirectory.string());
		return false;
	}

	error.clear();
	const std::filesystem::path workingDirectory = NormalizeProjectListPath(std::filesystem::current_path());
	if (!workingDirectory.empty() && PathIsOrIsUnder(workingDirectory, projectDirectory))
	{
		WHP_EDITOR_WARN(std::string("[Whip Hub] Refusing to delete a Project folder that contains the editor working directory: ") + projectDirectory.string());
		return false;
	}

	Ref<Project> activeProject = Project::GetActive();
	if (activeProject && PathsMatchForRecentProject(activeProject->GetProjectPath(), projectPath))
	{
		WHP_EDITOR_WARN("[Whip Hub] Refusing to delete the currently loaded Project.");
		return false;
	}

	std::filesystem::remove_all(projectDirectory, error);
	if (error)
	{
		WHP_EDITOR_ERROR(std::string("[Whip Hub] Could not delete Project folder ") + projectDirectory.string() + ": " + error.message());
		return false;
	}

	return ForgetRecentProject(layer, projectPath);
}

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

	return true;
}

std::filesystem::path EditorProjectManager::GetRecentProjectsPath() const
{
	return std::filesystem::current_path() / "WhipHubRecentProjects.txt";
}

std::filesystem::path EditorProjectManager::GetPreferencesPath() const
{
	return std::filesystem::current_path() / "WhipEditorPreferences.yaml";
}

void EditorProjectManager::LoadEditorPreferences(EditorLayer& layer)
{
	LoadRecentProjects(layer);

	const std::filesystem::path preferencesPath = GetPreferencesPath();
	std::error_code error;
	if (!std::filesystem::exists(preferencesPath, error))
		return;

	YAML::Node data;
	try
	{
		data = YAML::LoadFile(preferencesPath.string());
	}
	catch (const YAML::Exception& exception)
	{
		WHP_EDITOR_WARN(std::string("[Editor Preferences] Could not read preferences: ") + exception.what());
		return;
	}

	if (YAML::Node recentProjects = data["recentProjects"])
	{
		m_RecentProjects.clear();
		for (const YAML::Node& recentProject : recentProjects)
		{
			std::filesystem::path path = recentProject.as<std::string>("");
			if (!path.empty() && ShouldIncludeRecentProject(path))
				m_RecentProjects.push_back(path);
		}
	}

	m_LastProjectPath = data["last_project"].as<std::string>("");

	if (YAML::Node editor = data["editor"])
	{
		layer.m_UISettings.SetShowPhysicsColliders(editor["show_physics_colliders"].as<bool>(layer.m_UISettings.GetShowPhysicsColliders()));
		layer.m_UISettings.SetStepFrame(editor["step_frame"].as<int>(layer.m_UISettings.GetStepFrame()));
		layer.m_UISettings.SetTheme(ThemeFromString(editor["theme"].as<std::string>(UI::UISettings::GetThemeName(layer.m_UISettings.GetTheme()))));

		if (YAML::Node snap = editor["snap"])
		{
			layer.m_UISettings.SetSnapValues(0, ReadVec3(snap["translation"], layer.m_UISettings.GetSnapValues(0)));
			layer.m_UISettings.SetSnapValues(1, ReadVec3(snap["rotation"], layer.m_UISettings.GetSnapValues(1)));
			layer.m_UISettings.SetSnapValues(2, ReadVec3(snap["scale"], layer.m_UISettings.GetSnapValues(2)));
		}

		if (YAML::Node shortcuts = editor["shortcuts"])
		{
			for (size_t i = 0; i < UI::UISettings::ActionCount; ++i)
			{
				UI::EditorShortcutAction action = static_cast<UI::EditorShortcutAction>(i);
				YAML::Node shortcut = shortcuts[UI::UISettings::GetActionStorageKey(action)];
				if (!shortcut)
					continue;

				UI::ShortcutBinding binding;
				binding.m_Key = static_cast<KeyCode>(shortcut["key"].as<int>(0));
				binding.m_Ctrl = shortcut["ctrl"].as<bool>(false);
				binding.m_Shift = shortcut["shift"].as<bool>(false);
				binding.m_Alt = shortcut["alt"].as<bool>(false);
				layer.m_UISettings.SetShortcutBinding(action, binding);
			}
		}
	}

	if (YAML::Node panels = data["panels"])
	{
		layer.m_AnimationEditorPanel.SetOpen(panels["animation_editor"].as<bool>(layer.m_AnimationEditorPanel.IsOpen()));
		layer.m_SceneHierarchyPanel.SetOpen(panels["scene_hierarchy"].as<bool>(layer.m_SceneHierarchyPanel.IsOpen()));
		layer.m_UIStatistics.SetOpen(panels["statistics"].as<bool>(layer.m_UIStatistics.IsOpen()));
		ConsolePanel::SetOpen(panels["console"].as<bool>(ConsolePanel::IsOpen()));
	}

	if (YAML::Node browser = data["content_browser"])
	{
		m_ContentBrowserPreferences.m_ThumbnailSize = browser["thumbnail_size"].as<float>(m_ContentBrowserPreferences.m_ThumbnailSize);
		m_ContentBrowserPreferences.m_Padding = browser["padding"].as<float>(m_ContentBrowserPreferences.m_Padding);
		m_ContentBrowserPreferences.m_ShowUnsupported = browser["show_unsupported"].as<bool>(m_ContentBrowserPreferences.m_ShowUnsupported);
		m_ContentBrowserPreferences.m_Open = browser["open"].as<bool>(m_ContentBrowserPreferences.m_Open);
		m_ContentBrowserPreferences.m_Mode = browser["mode"].as<int>(m_ContentBrowserPreferences.m_Mode);
		m_ContentBrowserPreferences.m_TypeFilter = browser["type_filter"].as<int>(m_ContentBrowserPreferences.m_TypeFilter);
		m_ContentBrowserPreferences.m_CurrentDirectory = browser["current_directory"].as<std::string>("");
		m_HasContentBrowserPreferences = true;
	}

	layer.m_UISettings.ConsumeDirty();
	layer.m_SceneHierarchyPanel.ConsumeOpenDirty();
	layer.m_AnimationEditorPanel.ConsumeOpenDirty();
	layer.m_UIStatistics.ConsumeOpenDirty();
	ConsolePanel::ConsumeOpenDirty();
	m_ProjectLoader.SetRecentProjects(m_RecentProjects);
}

void EditorProjectManager::SaveEditorPreferences(const EditorLayer& layer) const
{
	YAML::Emitter out;
	out << YAML::BeginMap;
	out << YAML::Key << "last_project" << YAML::Value << m_LastProjectPath.string();
	out << YAML::Key << "recentProjects" << YAML::Value << YAML::BeginSeq;
	for (const auto& projectPath : m_RecentProjects)
		out << projectPath.string();
	out << YAML::EndSeq;

	out << YAML::Key << "editor" << YAML::Value << YAML::BeginMap;
	out << YAML::Key << "show_physics_colliders" << YAML::Value << layer.m_UISettings.GetShowPhysicsColliders();
	out << YAML::Key << "step_frame" << YAML::Value << layer.m_UISettings.GetStepFrame();
	out << YAML::Key << "theme" << YAML::Value << UI::UISettings::GetThemeName(layer.m_UISettings.GetTheme());
	out << YAML::Key << "snap" << YAML::Value << YAML::BeginMap;
	out << YAML::Key << "translation" << YAML::Value; WriteVec3(out, layer.m_UISettings.GetSnapValues(0));
	out << YAML::Key << "rotation" << YAML::Value; WriteVec3(out, layer.m_UISettings.GetSnapValues(1));
	out << YAML::Key << "scale" << YAML::Value; WriteVec3(out, layer.m_UISettings.GetSnapValues(2));
	out << YAML::EndMap;
	out << YAML::Key << "shortcuts" << YAML::Value << YAML::BeginMap;
	for (size_t i = 0; i < UI::UISettings::ActionCount; ++i)
	{
		UI::EditorShortcutAction action = static_cast<UI::EditorShortcutAction>(i);
		UI::ShortcutBinding binding = layer.m_UISettings.GetShortcutBinding(action);
		out << YAML::Key << UI::UISettings::GetActionStorageKey(action) << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "key" << YAML::Value << binding.m_Key;
		out << YAML::Key << "ctrl" << YAML::Value << binding.m_Ctrl;
		out << YAML::Key << "shift" << YAML::Value << binding.m_Shift;
		out << YAML::Key << "alt" << YAML::Value << binding.m_Alt;
		out << YAML::EndMap;
	}
	out << YAML::EndMap;
	out << YAML::EndMap;

	out << YAML::Key << "panels" << YAML::Value << YAML::BeginMap;
	out << YAML::Key << "animation_editor" << YAML::Value << layer.m_AnimationEditorPanel.IsOpen();
	out << YAML::Key << "scene_hierarchy" << YAML::Value << layer.m_SceneHierarchyPanel.IsOpen();
	out << YAML::Key << "statistics" << YAML::Value << layer.m_UIStatistics.IsOpen();
	out << YAML::Key << "console" << YAML::Value << ConsolePanel::IsOpen();
	out << YAML::EndMap;

	ContentBrowserPanel::Preferences browserPreferences = layer.m_ContentBrowserPanel ? layer.m_ContentBrowserPanel->GetPreferences() : m_ContentBrowserPreferences;
	out << YAML::Key << "content_browser" << YAML::Value << YAML::BeginMap;
	out << YAML::Key << "thumbnail_size" << YAML::Value << browserPreferences.m_ThumbnailSize;
	out << YAML::Key << "padding" << YAML::Value << browserPreferences.m_Padding;
	out << YAML::Key << "show_unsupported" << YAML::Value << browserPreferences.m_ShowUnsupported;
	out << YAML::Key << "open" << YAML::Value << browserPreferences.m_Open;
	out << YAML::Key << "mode" << YAML::Value << browserPreferences.m_Mode;
	out << YAML::Key << "type_filter" << YAML::Value << browserPreferences.m_TypeFilter;
	out << YAML::Key << "current_directory" << YAML::Value << browserPreferences.m_CurrentDirectory.string();
	out << YAML::EndMap;

	out << YAML::EndMap;

	std::ofstream stream(GetPreferencesPath(), std::ios::trunc);
	if (!stream)
		return;
	stream << out.c_str();
}

void EditorProjectManager::ApplyPreferencesToContentBrowser(EditorLayer& layer)
{
	if (layer.m_ContentBrowserPanel && m_HasContentBrowserPreferences)
		layer.m_ContentBrowserPanel->ApplyPreferences(m_ContentBrowserPreferences);
}

bool EditorProjectManager::NewProject(EditorLayer& layer, const UI::ProjectCreateSettings& settings)
{
	return layer.NewProject(settings);
}

void EditorProjectManager::SaveProject() const
{
	if (!Project::GetActive())
		return;

	Project::SaveActive();
}

void EditorProjectManager::FinishProjectSettings(EditorLayer& layer)
{
	if (!layer.HasProjectLoaded())
		return;

	Project::SaveActive();
	layer.m_ScriptManager.ReloadAssembly(true, layer.m_SceneState == EditorSceneState::Edit);
	layer.m_ContentBrowserPanel = MakeScope<ContentBrowserPanel>(Project::GetActive());
	layer.m_ContentBrowserPanel->SetAssetOpenCallback([&layer](AssetHandle handle) { return layer.m_AssetInteractionManager.HandleContentBrowserAssetOpen(layer, handle); });
	layer.m_ContentBrowserPanel->SetAssetInspectCallback([&layer](AssetHandle handle) { return layer.m_AssetInteractionManager.HandleContentBrowserAssetInspect(layer, handle); });
	ApplyPreferencesToContentBrowser(layer);
}

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
			{
				WHP_EDITOR_WARN(std::string("[Project] Could not migrate scene extension '") +
					legacyPath.string() + "' -> '" + modernPath.string() + "': " + error.message());
				continue;
			}
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

bool EditorProjectManager::OpenProject(EditorLayer& layer)
{
	return layer.OpenProject();
}

bool EditorProjectManager::OpenProject(EditorLayer& layer, const std::filesystem::path& path)
{
	return layer.OpenProject(path);
}

void EditorProjectManager::ResetEditorProjectState(EditorLayer& layer)
{
	layer.ResetEditorProjectState();
}

_WHIP_END
