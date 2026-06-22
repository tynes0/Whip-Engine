#include <Whip-Editor/Managers/EditorProjectManager.h>

#include <Whip-Editor/Managers/EditorScriptManager.h>
#include <Whip-Editor/Managers/EditorAssetInteractionManager.h>
#include <Whip-Editor/Managers/EditorSceneManager.h>
#include <Whip-Editor/Managers/EditorHistoryManager.h>
#include <Whip-Editor/Helpers/Utils.h>
#include <Whip-Editor/Panels/ConsolePanel.h>
#include <Whip-Editor/EditorLayer.h>

#include <Whip/Asset/AssetMetadata.h>
#include <Whip/Asset/SceneImporter.h>
#include <Whip/Scripting/ScriptEngine.h>
#include <Whip/Utils/FileExtensions.h>
#include <Whip/Utils/PlatformUtils.h>

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <fstream>
#include <string_view>

_WHIP_START

namespace
{
	void WriteVec3(YAML::Emitter& out, const glm::vec3& value)
	{
		out << YAML::Flow << YAML::BeginSeq << value.x << value.y << value.z << YAML::EndSeq;
	}

	void WriteVec2(YAML::Emitter& out, const glm::vec2& value)
	{
		out << YAML::Flow << YAML::BeginSeq << value.x << value.y << YAML::EndSeq;
	}

	glm::vec2 ReadVec2(const YAML::Node& node, const glm::vec2& fallback)
	{
		if (!node || !node.IsSequence() || node.size() != 2)
			return fallback;
		return { node[0].as<float>(fallback.x), node[1].as<float>(fallback.y) };
	}

	glm::vec3 ReadVec3(const YAML::Node& node, const glm::vec3& fallback)
	{
		if (!node || !node.IsSequence() || node.size() != 3)
			return fallback;
		return { node[0].as<float>(fallback.x), node[1].as<float>(fallback.y), node[2].as<float>(fallback.z) };
	}

	std::string SanitizeProjectToken(std::string value, const std::string& fallback)
	{
		std::erase_if(value, [](unsigned char c)
		{
			return !std::isalnum(c) && c != '_' && c != '-' && c != ' ';
		});

		while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
			value.erase(value.begin());
		while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
			value.pop_back();

		if (value.empty())
			value = fallback;
		return value;
	}

	std::string SanitizePathToken(std::string value, const std::string& fallback)
	{
		value = SanitizeProjectToken(std::move(value), fallback);
		for (char& c : value)
			if (c == ' ')
				c = '_';
		return value;
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

	bool CreateDirectoryChecked(const std::filesystem::path& path, std::string_view label)
	{
		std::error_code error;
		std::filesystem::create_directories(path, error);
		if (!error)
			return true;

		WHP_EDITOR_ERROR("[Whip Hub] Could not create {}: {} ({})", label, path.string(), error.message());
		return false;
	}
}

EditorProjectManager::EditorProjectManager(EditorLayer* boundedLayer)
	: EditorManagerBase(boundedLayer)
{
}

EditorProjectManager::~EditorProjectManager() = default;

UI::UIProjectLoader& EditorProjectManager::GetLoader()
{
	return m_ProjectLoader;
}

const UI::UIProjectLoader& EditorProjectManager::GetLoader() const
{
	return m_ProjectLoader;
}

const std::vector<std::filesystem::path>& EditorProjectManager::GetRecentProjects() const
{
	return m_RecentProjects;
}

void EditorProjectManager::SetupProjectLoader()
{
	m_ProjectLoader.SetCreateProjectCallback([this](const UI::ProjectCreateSettings& settings) { return NewProject(settings); });
	m_ProjectLoader.SetLoadProjectCallback([this]() { return OpenProject(); });
	m_ProjectLoader.SetOpenRecentProjectCallback([this](const std::filesystem::path& path) {
		std::error_code error;
		if (!std::filesystem::exists(path, error))
		{
			WHP_EDITOR_WARN("[Whip Hub] Recent Project no longer exists.");
			m_ProjectLoader.SetStatus("Recent project no longer exists.");
			LoadRecentProjects();
			return false;
		}

		const bool opened = OpenProject(path);
		if (!opened)
			m_ProjectLoader.SetStatus("Project could not be opened.");
		return opened;
	});
	m_ProjectLoader.SetForgetRecentProjectCallback([this](const std::filesystem::path& path) {
		return ForgetRecentProject(path);
	});
	m_ProjectLoader.SetDeleteRecentProjectCallback([this](const std::filesystem::path& path) {
		return DeleteRecentProject(path);
	});
}

void EditorProjectManager::LoadRecentProjects()
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

		if (std::ranges::find(m_RecentProjects, path) == m_RecentProjects.end())
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

void EditorProjectManager::AddRecentProject(const std::filesystem::path& path)
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

	std::erase(m_RecentProjects, normalizedPath);

	m_RecentProjects.insert(m_RecentProjects.begin(), normalizedPath);
	if (m_RecentProjects.size() > 10)
		m_RecentProjects.resize(10);

	m_LastProjectPath = normalizedPath;
	SaveRecentProjects();
	SaveEditorPreferences();
	m_ProjectLoader.SetRecentProjects(m_RecentProjects);
}

bool EditorProjectManager::ForgetRecentProject(const std::filesystem::path& path)
{
	if (path.empty())
		return false;

	const size_t previousSize = m_RecentProjects.size();
	std::erase_if(m_RecentProjects, [&path](const std::filesystem::path& recentPath)
	{
		return PathsMatchForRecentProject(recentPath, path);
	});

	if (m_RecentProjects.size() == previousSize)
		return false;

	if (PathsMatchForRecentProject(m_LastProjectPath, path))
		m_LastProjectPath.clear();

	SaveRecentProjects();
	SaveEditorPreferences();
	m_ProjectLoader.SetRecentProjects(m_RecentProjects);
	return true;
}

bool EditorProjectManager::DeleteRecentProject(const std::filesystem::path& path)
{
	if (path.empty() || !FileExtensions::IsProjectExtension(path))
		return false;

	std::error_code error;
	std::filesystem::path projectPath = NormalizeProjectListPath(path);
	const bool projectFileExists = std::filesystem::exists(projectPath, error);
	if (error)
		return false;
	if (!projectFileExists)
		return ForgetRecentProject(path);

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
	if (!workingDirectory.empty() && EditorUtils::PathIsOrIsUnder(workingDirectory, projectDirectory))
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

	return ForgetRecentProject(projectPath);
}

bool EditorProjectManager::ShouldIncludeRecentProject(const std::filesystem::path& path)
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

std::filesystem::path EditorProjectManager::GetRecentProjectsPath()
{
	return std::filesystem::current_path() / "WhipHubRecentProjects.txt";
}

std::filesystem::path EditorProjectManager::GetPreferencesPath()
{
	return std::filesystem::current_path() / "WhipEditorPreferences.yaml";
}

void EditorProjectManager::LoadEditorPreferences()
{
	EditorLayer& layer = GetLayer();
	LoadRecentProjects();

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
		if (YAML::Node assistant = editor["assistant"])
		{
			Assistant::Settings settings = layer.m_UISettings.GetAssistantSettings();
			settings.m_Enabled = assistant["enabled"].as<bool>(settings.m_Enabled);
			settings.m_Provider = Assistant::ProviderFromName(assistant["provider"].as<std::string>(""));
			if (!assistant["provider"] && assistant["online"].as<bool>(false))
				settings.m_Provider = Assistant::ProviderKind::OpenAI;
			settings.m_SendSceneContext = assistant["send_scene_context"].as<bool>(settings.m_SendSceneContext);
			settings.m_SendConsoleContext = assistant["send_console_context"].as<bool>(settings.m_SendConsoleContext);
			settings.m_OpenAIModel = assistant["openai_model"].as<std::string>(assistant["model"].as<std::string>(settings.m_OpenAIModel));
			settings.m_OpenAIApiKey = assistant["openai_api_key"].as<std::string>(assistant["api_key"].as<std::string>(settings.m_OpenAIApiKey));
			settings.m_GeminiModel = assistant["gemini_model"].as<std::string>(settings.m_GeminiModel);
			settings.m_GeminiApiKey = assistant["gemini_api_key"].as<std::string>(settings.m_GeminiApiKey);
			settings.m_GeminiUseGoogleSearch = assistant["gemini_google_search"].as<bool>(settings.m_GeminiUseGoogleSearch);
			layer.m_UISettings.SetAssistantSettings(settings);
		}

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
			layer.m_ShortcutManager.SyncLegacyGlobalBindings(layer.m_UISettings);
		}

		if (YAML::Node shortcutManager = editor["shortcut_manager"])
			layer.m_ShortcutManager.LoadBindings(shortcutManager);
	}

	if (YAML::Node panels = data["panels"])
	{
		layer.m_AnimationEditorPanel.SetOpen(panels["animation_editor"].as<bool>(layer.m_AnimationEditorPanel.IsOpen()));
		layer.m_SceneHierarchyPanel.SetOpen(panels["scene_hierarchy"].as<bool>(layer.m_SceneHierarchyPanel.IsOpen()));
		layer.m_UIStatistics.SetOpen(panels["statistics"].as<bool>(layer.m_UIStatistics.IsOpen()));
		ConsolePanel::SetOpen(panels["console"].as<bool>(ConsolePanel::IsOpen()));
		layer.m_AssistantPanel.SetOpen(panels["assistant"].as<bool>(layer.m_AssistantPanel.IsOpen()));

		if (YAML::Node animationWorkspace = panels["animation_workspace"])
		{
			AnimationEditorPanel::WorkspacePreferences preferences = layer.m_AnimationEditorPanel.GetWorkspacePreferences();
			preferences.m_Open = animationWorkspace["open"].as<bool>(preferences.m_Open);
			preferences.m_Minimized = animationWorkspace["minimized"].as<bool>(preferences.m_Minimized);
			preferences.m_Fullscreen = animationWorkspace["fullscreen"].as<bool>(preferences.m_Fullscreen);
			preferences.m_HasRestoreRect = animationWorkspace["has_restore_rect"].as<bool>(preferences.m_HasRestoreRect);
			preferences.m_RestorePosition = ReadVec2(animationWorkspace["restore_position"], preferences.m_RestorePosition);
			preferences.m_RestoreSize = ReadVec2(animationWorkspace["restore_size"], preferences.m_RestoreSize);
			layer.m_AnimationEditorPanel.ApplyWorkspacePreferences(preferences);
		}

		if (YAML::Node assetWorkspace = panels["asset_workspace"])
		{
			AssetEditorPanel::WorkspacePreferences preferences = layer.m_AssetEditorPanel.GetWorkspacePreferences();
			preferences.m_Open = assetWorkspace["open"].as<bool>(preferences.m_Open);
			preferences.m_Minimized = assetWorkspace["minimized"].as<bool>(preferences.m_Minimized);
			preferences.m_Fullscreen = assetWorkspace["fullscreen"].as<bool>(preferences.m_Fullscreen);
			preferences.m_HasRestoreRect = assetWorkspace["has_restore_rect"].as<bool>(preferences.m_HasRestoreRect);
			preferences.m_RestorePosition = ReadVec2(assetWorkspace["restore_position"], preferences.m_RestorePosition);
			preferences.m_RestoreSize = ReadVec2(assetWorkspace["restore_size"], preferences.m_RestoreSize);
			layer.m_AssetEditorPanel.ApplyWorkspacePreferences(preferences);
		}
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
	layer.m_ShortcutManager.ConsumeDirty();
	layer.m_SceneHierarchyPanel.ConsumeOpenDirty();
	layer.m_AnimationEditorPanel.ConsumeOpenDirty();
	layer.m_AnimationEditorPanel.ConsumeLayoutDirty();
	layer.m_AssetEditorPanel.ConsumeLayoutDirty();
	layer.m_UIStatistics.ConsumeOpenDirty();
	layer.m_AssistantPanel.ConsumeOpenDirty();
	ConsolePanel::ConsumeOpenDirty();
	m_ProjectLoader.SetRecentProjects(m_RecentProjects);
}

void EditorProjectManager::SaveEditorPreferences() const
{
	const EditorLayer& layer = GetLayer();

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
	{
		const Assistant::Settings& assistant = layer.m_UISettings.GetAssistantSettings();
		out << YAML::Key << "assistant" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "enabled" << YAML::Value << assistant.m_Enabled;
		out << YAML::Key << "provider" << YAML::Value << Assistant::ProviderName(assistant.m_Provider);
		out << YAML::Key << "send_scene_context" << YAML::Value << assistant.m_SendSceneContext;
		out << YAML::Key << "send_console_context" << YAML::Value << assistant.m_SendConsoleContext;
		out << YAML::Key << "openai_model" << YAML::Value << assistant.m_OpenAIModel;
		out << YAML::Key << "openai_api_key" << YAML::Value << assistant.m_OpenAIApiKey;
		out << YAML::Key << "gemini_model" << YAML::Value << assistant.m_GeminiModel;
		out << YAML::Key << "gemini_api_key" << YAML::Value << assistant.m_GeminiApiKey;
		out << YAML::Key << "gemini_google_search" << YAML::Value << assistant.m_GeminiUseGoogleSearch;
		out << YAML::EndMap;
	}
	out << YAML::Key << "snap" << YAML::Value << YAML::BeginMap;
	out << YAML::Key << "translation" << YAML::Value; WriteVec3(out, layer.m_UISettings.GetSnapValues(0));
	out << YAML::Key << "rotation" << YAML::Value; WriteVec3(out, layer.m_UISettings.GetSnapValues(1));
	out << YAML::Key << "scale" << YAML::Value; WriteVec3(out, layer.m_UISettings.GetSnapValues(2));
	out << YAML::EndMap;
	out << YAML::Key << "shortcuts" << YAML::Value << YAML::BeginMap;
	for (size_t i = 0; i < UI::UISettings::ActionCount; ++i)
	{
		UI::EditorShortcutAction action = static_cast<UI::EditorShortcutAction>(i);
		const std::string shortcutId = std::string("global.") + UI::UISettings::GetActionStorageKey(action);
		UI::ShortcutBinding binding = layer.m_ShortcutManager.GetBinding(shortcutId, layer.m_UISettings.GetShortcutBinding(action));
		out << YAML::Key << UI::UISettings::GetActionStorageKey(action) << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "key" << YAML::Value << binding.m_Key;
		out << YAML::Key << "ctrl" << YAML::Value << binding.m_Ctrl;
		out << YAML::Key << "shift" << YAML::Value << binding.m_Shift;
		out << YAML::Key << "alt" << YAML::Value << binding.m_Alt;
		out << YAML::EndMap;
	}
	out << YAML::EndMap;
	out << YAML::Key << "shortcut_manager" << YAML::Value;
	layer.m_ShortcutManager.SaveBindings(out);
	out << YAML::EndMap;

	out << YAML::Key << "panels" << YAML::Value << YAML::BeginMap;
	out << YAML::Key << "animation_editor" << YAML::Value << layer.m_AnimationEditorPanel.IsOpen();
	out << YAML::Key << "scene_hierarchy" << YAML::Value << layer.m_SceneHierarchyPanel.IsOpen();
	out << YAML::Key << "statistics" << YAML::Value << layer.m_UIStatistics.IsOpen();
	out << YAML::Key << "console" << YAML::Value << ConsolePanel::IsOpen();
	out << YAML::Key << "assistant" << YAML::Value << layer.m_AssistantPanel.IsOpen();
	{
		const AnimationEditorPanel::WorkspacePreferences preferences = layer.m_AnimationEditorPanel.GetWorkspacePreferences();
		out << YAML::Key << "animation_workspace" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "open" << YAML::Value << preferences.m_Open;
		out << YAML::Key << "minimized" << YAML::Value << preferences.m_Minimized;
		out << YAML::Key << "fullscreen" << YAML::Value << preferences.m_Fullscreen;
		out << YAML::Key << "has_restore_rect" << YAML::Value << preferences.m_HasRestoreRect;
		out << YAML::Key << "restore_position" << YAML::Value; WriteVec2(out, preferences.m_RestorePosition);
		out << YAML::Key << "restore_size" << YAML::Value; WriteVec2(out, preferences.m_RestoreSize);
		out << YAML::EndMap;
	}
	{
		const AssetEditorPanel::WorkspacePreferences preferences = layer.m_AssetEditorPanel.GetWorkspacePreferences();
		out << YAML::Key << "asset_workspace" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "open" << YAML::Value << preferences.m_Open;
		out << YAML::Key << "minimized" << YAML::Value << preferences.m_Minimized;
		out << YAML::Key << "fullscreen" << YAML::Value << preferences.m_Fullscreen;
		out << YAML::Key << "has_restore_rect" << YAML::Value << preferences.m_HasRestoreRect;
		out << YAML::Key << "restore_position" << YAML::Value; WriteVec2(out, preferences.m_RestorePosition);
		out << YAML::Key << "restore_size" << YAML::Value; WriteVec2(out, preferences.m_RestoreSize);
		out << YAML::EndMap;
	}
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

void EditorProjectManager::ApplyPreferencesToContentBrowser()
{
	EditorLayer& layer = GetLayer();
	if (layer.m_ContentBrowserPanel && m_HasContentBrowserPreferences)
		layer.m_ContentBrowserPanel->ApplyPreferences(m_ContentBrowserPreferences);
}

void EditorProjectManager::SaveProject() const
{
	if (!Project::GetActive())
		return;

	Project::SaveActive();
}

bool EditorProjectManager::NewProject(const UI::ProjectCreateSettings& settings)
{
	const std::string projectName = SanitizeProjectToken(settings.m_Name, "Untitled");
	const std::string projectFolderName = SanitizePathToken(projectName, "Untitled");
	const std::string initialSceneName = SanitizePathToken(settings.m_InitialSceneName, "Main");
	if (settings.m_Location.empty())
		return false;

	std::filesystem::path projectDirectory = settings.m_Location / projectFolderName;
	std::filesystem::path projectPath = projectDirectory / (projectFolderName + FileExtensions::Project);
	std::error_code error;
	if (std::filesystem::exists(projectPath, error))
	{
		WHP_EDITOR_WARN(std::string("[Whip Hub] Project file already exists: ") + projectPath.string());
		return false;
	}

	if (!CreateDirectoryChecked(projectDirectory / "Assets" / "Scenes", "Project scenes directory") ||
		!CreateDirectoryChecked(projectDirectory / "Assets" / "Scripts" / "Source", "script source directory") ||
		!CreateDirectoryChecked(projectDirectory / "Assets" / "Scripts" / "Binaries", "script binaries directory") ||
		!CreateDirectoryChecked(projectDirectory / "Assets" / "Animations", "animations directory") ||
		!CreateDirectoryChecked(projectDirectory / "Assets" / "Audios", "audio directory") ||
		!CreateDirectoryChecked(projectDirectory / "Assets" / "fonts", "font directory") ||
		!CreateDirectoryChecked(projectDirectory / "Assets" / "textures", "texture directory"))
	{
		return false;
	}

	Ref<Project> newProject = Project::NewProject();
	Project::SetActiveProjectPath(projectPath);

	ProjectConfig& config = newProject->GetConfig();
	config.m_Name = projectName;
	config.m_AssetDirectory = "Assets";
	config.m_AssetRegistryPath = FileExtensions::AssetRegistryFilename;
	config.m_ScriptModulePath = std::filesystem::path("Scripts") / "Binaries" / (projectFolderName + ".dll");
	config.m_StartScene = 0;

	if (!EditorScriptManager::WriteProjectFiles(projectDirectory, projectFolderName))
	{
		Project::SetActive(nullptr);
		return false;
	}

	std::filesystem::path startSceneRelativePath;
	AssetHandle startSceneHandle = 0;
	if (settings.m_CreateStartScene)
	{
		startSceneHandle = AssetHandle{};
		config.m_StartScene = startSceneHandle;
		startSceneRelativePath = std::filesystem::path("Scenes") / (initialSceneName + FileExtensions::Scene);

		Ref<Scene> startScene = MakeRef<Scene>(startSceneHandle);
		if (settings.m_TemplateIndex == 1 || settings.m_TemplateIndex == 2)
		{
			Entity camera = startScene->CreateEntity("Main Camera");
			camera.AddComponent<CameraComponent>();
			camera.GetComponent<TransformComponent>().m_Translation = { 0.0f, 0.0f, 8.0f };

			Entity sprite = startScene->CreateEntity(settings.m_TemplateIndex == 2 ? "Starter Entity" : "Sprite");
			sprite.AddComponent<SpriteRendererComponent>(glm::vec4{ 0.86f, 0.58f, 0.28f, 1.0f });
			if (settings.m_TemplateIndex == 2)
				sprite.AddComponent<ScriptComponent>().m_ClassName = projectFolderName + ".StarterEntity";
		}

		SceneImporter::SaveScene(startScene, projectDirectory / config.m_AssetDirectory / startSceneRelativePath);
	}

	if (!Project::SaveActive(projectPath))
	{
		Project::SetActive(nullptr);
		return false;
	}

	std::ofstream registry(projectPath.parent_path() / config.m_AssetDirectory / config.m_AssetRegistryPath, std::ios::trunc);
	if (!registry)
	{
		WHP_EDITOR_ERROR("[Whip Hub] Could not write Asset registry.");
		Project::SetActive(nullptr);
		return false;
	}

	if (settings.m_CreateStartScene)
	{
		registry << "AssetRegistry:\n";
		registry << "  - handle: " << (uint64_t)startSceneHandle << '\n';
		registry << "    filepath: " << startSceneRelativePath.generic_string() << '\n';
		registry << "    type: scene\n";
	}
	else
	{
		registry << "AssetRegistry: []\n";
	}
	registry.close();

	Project::SetActive(nullptr);
	if (!settings.m_OpenAfterCreate)
	{
		AddRecentProject(projectPath);
		return true;
	}
	return OpenProject(projectPath);
}

void EditorProjectManager::FinishProjectSettings()
{
	EditorLayer& layer = GetLayer();
	if (!layer.HasProjectLoaded())
		return;

	Project::SaveActive();
	layer.m_ScriptManager.ReloadAssembly(true, layer.m_SceneManager.State() == EditorSceneState::Edit);
	layer.m_ContentBrowserPanel = MakeScope<ContentBrowserPanel>(Project::GetActive());
	layer.m_ContentBrowserPanel->SetAssetOpenCallback([this](AssetHandle handle) { return GetLayer().m_AssetInteractionManager.HandleContentBrowserAssetOpen(handle); });
	layer.m_ContentBrowserPanel->SetAssetInspectCallback([this](AssetHandle handle) { return GetLayer().m_AssetInteractionManager.HandleContentBrowserAssetInspect(handle); });
	ApplyPreferencesToContentBrowser();
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

bool EditorProjectManager::OpenProject()
{
	std::string filepath = FileDialogs::OpenFile("Whip Project (*.wproj)\0*.wproj\0");
	if (filepath.empty())
		return false;
	return OpenProject(filepath);
}

bool EditorProjectManager::OpenProject(const std::filesystem::path& path)
{
	EditorLayer& layer = GetLayer();
	if (path.empty())
		return false;

	std::filesystem::path projectPath = NormalizeProjectListPath(path);
	std::error_code error;
	if (!std::filesystem::exists(projectPath, error) || !FileExtensions::IsProjectExtension(projectPath))
	{
		WHP_EDITOR_WARN(std::string("[Project] Project file is missing or invalid: ") + projectPath.string());
		m_ProjectLoader.SetStatus("Project file is missing or invalid.");
		return false;
	}

	if (layer.HasProjectLoaded() && PathsMatchForRecentProject(Project::GetActive()->GetProjectPath(), projectPath))
	{
		WHP_EDITOR_INFO(std::string("[Project] Project is already open: ") + projectPath.string());
		AddRecentProject(projectPath);
		m_ProjectLoader.SetLoaded(true);
		m_ProjectLoader.SetStatus("Project already open.");
		return true;
	}

	WHP_EDITOR_INFO(std::string("[Project] Opening Project: ") + projectPath.string());
	if (layer.HasProjectLoaded())
	{
		WHP_EDITOR_INFO("[Project] Unloading current Project before opening a new one.");
		ResetEditorProjectState();
	}

	if (Project::Load(projectPath))
	{
		WHP_EDITOR_INFO("[Project] Project file loaded.");
		MigrateProjectNativeFileExtensions();
		WHP_EDITOR_INFO("[Project] Native file extension migration complete.");
		const bool scriptBuildSucceeded = layer.m_ScriptManager.BuildProjectScripts();
		if (!scriptBuildSucceeded)
			WHP_EDITOR_WARN("[Script Build] Project opened, but script build failed.");
		WHP_EDITOR_INFO("[Project] Script build step complete.");
		ScriptEngine::Init();
		WHP_EDITOR_INFO("[Project] Script engine initialized.");
		layer.m_ScriptManager.StartSourceWatcher();
		AssetHandle startScene = (Project::GetActive()->GetConfig().m_StartScene);
		if (startScene && Project::GetActive()->GetEditorAssetManager()->IsAssetHandleValid(startScene))
		{
			const std::filesystem::path startScenePath = Project::GetActive()->GetEditorAssetManager()->GetFilepath(startScene);
			if (std::filesystem::exists(Project::GetActiveAssetDirectory() / startScenePath))
				layer.m_SceneManager.OpenScene(startScene);
			else
			{
				WHP_EDITOR_WARN("[Project] Start scene file is missing. Resetting Project start scene.");
				Project::GetActive()->GetConfig().m_StartScene = 0;
				Project::SaveActive();
			}
		}
		else if (startScene)
		{
			WHP_EDITOR_WARN("[Project] Start scene is missing. Resetting Project start scene.");
			Project::GetActive()->GetConfig().m_StartScene = 0;
			Project::SaveActive();
		}
		else
		{
			layer.m_SceneManager.NewScene();
		}
		layer.m_ContentBrowserPanel = MakeScope<ContentBrowserPanel>(Project::GetActive());
		layer.m_ContentBrowserPanel->SetAssetOpenCallback([this](AssetHandle handle) { return GetLayer().m_AssetInteractionManager.HandleContentBrowserAssetOpen(handle); });
		layer.m_ContentBrowserPanel->SetAssetInspectCallback([this](AssetHandle handle) { return GetLayer().m_AssetInteractionManager.HandleContentBrowserAssetInspect(handle); });
		ApplyPreferencesToContentBrowser();
		AddRecentProject(projectPath);
		m_ProjectLoader.SetLoaded(true);
		m_ProjectLoader.SetStatus(scriptBuildSucceeded ? "Project opened." : "Project opened, script build failed.");
		WHP_EDITOR_INFO("[Project] Project open complete.");
		return true;
	}
	ResetEditorProjectState();
	WHP_EDITOR_WARN(std::string("[Project] Project load failed: ") + projectPath.string());
	m_ProjectLoader.SetStatus("Project could not be opened.");
	return false;
}

void EditorProjectManager::ResetEditorProjectState()
{
	EditorLayer& layer = GetLayer();
	layer.m_SceneManager.WriteRecoverySnapshot("Project switch");
	layer.m_ScriptManager.StopSourceWatcher();
	if (layer.m_SceneManager.State() != EditorSceneState::Edit)
		layer.m_SceneManager.OnSceneStop();

	ScriptEngine::ClearRuntimeSceneTransitionRequest();
	layer.m_ScriptManager.Reset();

	layer.m_ContentBrowserPanel.reset();
	layer.m_SceneHierarchyPanel.SetContext({});
	layer.m_SceneManager.ResetToEmptyScene();
	layer.m_HistoryManager.ClearSceneHistory();
	Project::SetActive(nullptr);
	m_ProjectLoader.SetLoaded(false);
}

_WHIP_END
