#include <Whip-Editor/Managers/EditorProjectManager.h>

#include <Whip-Editor/Managers/EditorScriptManager.h>
#include <Whip-Editor/Managers/EditorAssetInteractionManager.h>
#include <Whip-Editor/Managers/EditorSceneManager.h>
#include <Whip-Editor/Managers/EditorHistoryManager.h>
#include <Whip-Editor/Helpers/Utils.h>
#include <Whip-Editor/Panels/ConsolePanel.h>
#include <Whip-Editor/EditorLayer.h>

#include <Whip/Debug/Instrumentor.h>
#include <Whip/Asset/AssetMetadata.h>
#include <Whip/Asset/SceneImporter.h>
#include <Whip/Scripting/ScriptEngine.h>
#include <Whip/Utils/FileExtensions.h>
#include <Whip/Utils/PlatformUtils.h>

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <array>
#include <fstream>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

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

	enum class ProjectTemplateKind
	{
		Empty = 0,
		Starter2D,
		ScriptReady
	};

	ProjectTemplateKind ProjectTemplateFromIndex(int index)
	{
		switch (index)
		{
		case 0: return ProjectTemplateKind::Empty;
		case 2: return ProjectTemplateKind::ScriptReady;
		case 1:
		default: return ProjectTemplateKind::Starter2D;
		}
	}

	const char* ProjectTemplateName(ProjectTemplateKind kind)
	{
		switch (kind)
		{
		case ProjectTemplateKind::Empty: return "Empty";
		case ProjectTemplateKind::Starter2D: return "2D Starter";
		case ProjectTemplateKind::ScriptReady: return "Script Ready";
		}
		return "Unknown";
	}

	bool WriteTextFileChecked(const std::filesystem::path& path, std::string_view contents, std::string_view label)
	{
		std::error_code error;
		std::filesystem::create_directories(path.parent_path(), error);
		if (error)
		{
			WHP_EDITOR_ERROR("[Whip Hub] Could not create {} directory: {} ({})", label, path.parent_path().string(), error.message());
			return false;
		}

		std::ofstream stream(path, std::ios::binary | std::ios::trunc);
		if (!stream)
		{
			WHP_EDITOR_ERROR("[Whip Hub] Could not write {}: {}", label, path.string());
			return false;
		}

		stream << contents;
		return true;
	}

	bool EnsureProjectScaffoldDirectories(const std::filesystem::path& projectDirectory)
	{
		const std::array<std::filesystem::path, 10> directories =
		{
			std::filesystem::path("Assets") / "Scenes",
			std::filesystem::path("Assets") / "Scripts" / "Source",
			std::filesystem::path("Assets") / "Scripts" / "Binaries",
			std::filesystem::path("Assets") / "Scripts" / "Intermediates",
			std::filesystem::path("Assets") / "Animations",
			std::filesystem::path("Assets") / "Audios",
			std::filesystem::path("Assets") / "fonts",
			std::filesystem::path("Assets") / "textures",
			std::filesystem::path("Assets") / "Prefabs",
			std::filesystem::path("Assets") / "UI"
		};

		for (const std::filesystem::path& directory : directories)
		{
			if (!CreateDirectoryChecked(projectDirectory / directory, directory.generic_string()))
				return false;
		}

		return true;
	}

	void WriteProjectLocalGitIgnore(const std::filesystem::path& projectDirectory)
	{
		const std::filesystem::path gitignorePath = projectDirectory / ".gitignore";
		std::error_code error;
		if (std::filesystem::exists(gitignorePath, error))
			return;

		constexpr std::string_view contents =
			"# Whip generated project cache\n"
			"Assets/Scripts/Binaries/\n"
			"Assets/Scripts/Intermediates/\n"
			"Assets/Scripts/**/bin/\n"
			"Assets/Scripts/**/obj/\n"
			"Assets/Scripts/.vs/\n"
			"Assets/Scripts/.idea/\n"
			"*.user\n"
			"*.suo\n";

		if (!WriteTextFileChecked(gitignorePath, contents, "Project .gitignore"))
			WHP_EDITOR_WARN("[Whip Hub] Project was created, but local .gitignore could not be written.");
	}

	Entity CreateTemplateCamera(const Ref<Scene>& scene)
	{
		Entity camera = scene->CreateEntity("Main Camera");
		CameraComponent& cameraComponent = camera.AddComponent<CameraComponent>();
		cameraComponent.m_Primary = true;
		cameraComponent.m_Camera.SetProjectionType(SceneCamera::ProjectionType::Orthographic);
		cameraComponent.m_Camera.SetOrthographicSize(10.0f);
		camera.GetComponent<TransformComponent>().m_Translation = { 0.0f, 0.0f, 8.0f };
		return camera;
	}

	Entity CreateTemplateSprite(const Ref<Scene>& scene, std::string_view name, const glm::vec4& color, const glm::vec3& translation)
	{
		Entity sprite = scene->CreateEntity(std::string(name));
		sprite.GetComponent<TransformComponent>().m_Translation = translation;
		sprite.AddComponent<SpriteRendererComponent>(color);
		return sprite;
	}

	void PopulateTemplateScene(const Ref<Scene>& scene, ProjectTemplateKind kind, const std::string& scriptNamespace)
	{
		CreateTemplateCamera(scene);

		if (kind == ProjectTemplateKind::Empty)
			return;

		Entity starter = CreateTemplateSprite(scene,
			kind == ProjectTemplateKind::ScriptReady ? "Starter Entity" : "Sprite",
			{ 0.86f, 0.58f, 0.28f, 1.0f },
			{ 0.0f, 0.0f, 0.0f });
		starter.GetComponent<TransformComponent>().m_Scale = { 1.5f, 1.5f, 1.0f };

		if (kind == ProjectTemplateKind::ScriptReady)
			starter.AddComponent<ScriptComponent>().m_ClassName = scriptNamespace + ".StarterEntity";
	}

	bool WriteStarterAssetRegistry(
		const std::filesystem::path& registryPath,
		std::optional<std::pair<AssetHandle, std::filesystem::path>> startScene)
	{
		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "asset_registry" << YAML::Value << YAML::BeginSeq;
		if (startScene)
		{
			out << YAML::BeginMap;
			out << YAML::Key << "handle" << YAML::Value << static_cast<uint64_t>(startScene->first);
			out << YAML::Key << "filepath" << YAML::Value << startScene->second.generic_string();
			out << YAML::Key << "type" << YAML::Value << frenum::to_string(AssetType::Scene);
			out << YAML::EndMap;
		}
		out << YAML::EndSeq;
		out << YAML::EndMap;

		return WriteTextFileChecked(registryPath, out.c_str(), "Asset registry");
	}

	bool EnsureAssetRegistryFileExists(const Ref<Project>& project)
	{
		if (!project)
			return false;

		const std::filesystem::path registryPath = project->GetAssetRegistryPath();
		std::error_code error;
		if (std::filesystem::exists(registryPath, error))
			return true;

		if (!WriteStarterAssetRegistry(registryPath, std::nullopt))
		{
			WHP_EDITOR_ERROR("[Project] Could not create missing Asset registry.");
			return false;
		}

		WHP_EDITOR_INFO("[Project] Created missing Asset registry.");
		return true;
	}

	AssetHandle FindFirstAvailableSceneHandle(const Ref<Project>& project)
	{
		if (!project || !project->GetEditorAssetManager())
			return 0;

		std::vector<std::pair<AssetHandle, std::filesystem::path>> scenes;
		const auto& registry = project->GetEditorAssetManager()->GetAssetRegistry().GetFiltered(AssetType::Scene);
		scenes.reserve(registry.size());
		for (const auto& [handle, metadata] : registry)
			scenes.emplace_back(handle, metadata.m_Filepath);

		std::ranges::sort(scenes, [](const auto& left, const auto& right)
		{
			return left.second.generic_string() < right.second.generic_string();
		});

		for (const auto& [handle, relativePath] : scenes)
		{
			std::error_code error;
			if (std::filesystem::exists(project->GetAssetDirectory() / relativePath, error))
				return handle;
		}
		return 0;
	}

	AssetHandle ResolveStartSceneHandle(const Ref<Project>& project)
	{
		if (!project || !project->GetEditorAssetManager())
			return 0;

		const AssetHandle configuredStartScene = project->GetConfig().m_StartScene;
		if (configuredStartScene != 0 && project->GetEditorAssetManager()->IsAssetHandleValid(configuredStartScene))
		{
			const std::filesystem::path startScenePath = project->GetEditorAssetManager()->GetFilepath(configuredStartScene);
			std::error_code error;
			if (std::filesystem::exists(project->GetAssetDirectory() / startScenePath, error))
				return configuredStartScene;
		}

		return FindFirstAvailableSceneHandle(project);
	}

	bool RepairLoadedProjectScaffold(const Ref<Project>& project)
	{
		if (!project)
			return false;

		bool changed = false;
		const std::filesystem::path projectDirectory = project->GetProjectDirectory();
		ProjectConfig& config = project->GetConfig();
		if (config.m_AssetDirectory.empty())
		{
			config.m_AssetDirectory = "Assets";
			changed = true;
		}
		if (config.m_AssetRegistryPath.empty())
		{
			config.m_AssetRegistryPath = FileExtensions::AssetRegistryFilename;
			changed = true;
		}
		if (config.m_ScriptModulePath.empty())
		{
			const std::string projectFolderName = SanitizePathToken(config.m_Name, project->GetProjectPath().stem().string());
			config.m_ScriptModulePath = std::filesystem::path("Scripts") / "Binaries" / (projectFolderName + ".dll");
			changed = true;
		}

		if (changed)
			Project::SaveActive();

		EnsureProjectScaffoldDirectories(projectDirectory);
		WriteProjectLocalGitIgnore(projectDirectory);
		EnsureAssetRegistryFileExists(project);
		return changed;
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
	WHP_PROFILE_FUNCTION();
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
	WHP_PROFILE_FUNCTION();
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
	WHP_PROFILE_FUNCTION();
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
	WHP_PROFILE_FUNCTION();
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
		layer.m_UISettings.SetShowEditorGrid(editor["show_editor_grid"].as<bool>(layer.m_UISettings.GetShowEditorGrid()));
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
			settings.m_SendAssetImages = assistant["send_asset_images"].as<bool>(settings.m_SendAssetImages);
			settings.m_ApplyMode = Assistant::ApplyModeFromName(assistant["apply_mode"].as<std::string>(Assistant::ApplyModeName(settings.m_ApplyMode)));
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
		layer.m_ProjectHealthPanel.SetOpen(panels["project_health"].as<bool>(layer.m_ProjectHealthPanel.IsOpen()));

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
	layer.m_ProjectHealthPanel.ConsumeOpenDirty();
	ConsolePanel::ConsumeOpenDirty();
	m_ProjectLoader.SetRecentProjects(m_RecentProjects);
}

void EditorProjectManager::SaveEditorPreferences() const
{
	WHP_PROFILE_FUNCTION();
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
	out << YAML::Key << "show_editor_grid" << YAML::Value << layer.m_UISettings.GetShowEditorGrid();
	out << YAML::Key << "step_frame" << YAML::Value << layer.m_UISettings.GetStepFrame();
	out << YAML::Key << "theme" << YAML::Value << UI::UISettings::GetThemeName(layer.m_UISettings.GetTheme());
	{
		const Assistant::Settings& assistant = layer.m_UISettings.GetAssistantSettings();
		out << YAML::Key << "assistant" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "enabled" << YAML::Value << assistant.m_Enabled;
		out << YAML::Key << "provider" << YAML::Value << Assistant::ProviderName(assistant.m_Provider);
		out << YAML::Key << "send_scene_context" << YAML::Value << assistant.m_SendSceneContext;
		out << YAML::Key << "send_console_context" << YAML::Value << assistant.m_SendConsoleContext;
		out << YAML::Key << "send_asset_images" << YAML::Value << assistant.m_SendAssetImages;
		out << YAML::Key << "apply_mode" << YAML::Value << Assistant::ApplyModeName(assistant.m_ApplyMode);
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
	out << YAML::Key << "project_health" << YAML::Value << layer.m_ProjectHealthPanel.IsOpen();
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
	WHP_PROFILE_FUNCTION();
	if (!Project::GetActive())
		return;

	Project::SaveActive();
}

bool EditorProjectManager::NewProject(const UI::ProjectCreateSettings& settings)
{
	WHP_PROFILE_FUNCTION();
	const std::string projectName = SanitizeProjectToken(settings.m_Name, "Untitled");
	const std::string projectFolderName = SanitizePathToken(projectName, "Untitled");
	const std::string initialSceneName = SanitizePathToken(settings.m_InitialSceneName, "Main");
	const ProjectTemplateKind templateKind = ProjectTemplateFromIndex(settings.m_TemplateIndex);
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

	if (std::filesystem::exists(projectDirectory, error) && !std::filesystem::is_empty(projectDirectory, error))
	{
		WHP_EDITOR_WARN(std::string("[Whip Hub] Project folder is not empty: ") + projectDirectory.string());
		return false;
	}

	if (!EnsureProjectScaffoldDirectories(projectDirectory))
	{
		return false;
	}
	WriteProjectLocalGitIgnore(projectDirectory);

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
		PopulateTemplateScene(startScene, templateKind, projectFolderName);

		SceneImporter::SaveScene(startScene, projectDirectory / config.m_AssetDirectory / startSceneRelativePath);
	}

	if (!Project::SaveActive(projectPath))
	{
		Project::SetActive(nullptr);
		return false;
	}

	if (!WriteStarterAssetRegistry(
		projectPath.parent_path() / config.m_AssetDirectory / config.m_AssetRegistryPath,
		settings.m_CreateStartScene ? std::optional<std::pair<AssetHandle, std::filesystem::path>>{ { startSceneHandle, startSceneRelativePath } } : std::nullopt))
	{
		WHP_EDITOR_ERROR("[Whip Hub] Could not write Asset registry.");
		Project::SetActive(nullptr);
		return false;
	}

	WHP_EDITOR_INFO(std::string("[Whip Hub] Created ") + ProjectTemplateName(templateKind) + " Project: " + projectPath.string());

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
	layer.RegisterEditorShortcuts();
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
	WHP_PROFILE_FUNCTION();
	std::string filepath = FileDialogs::OpenFile("Whip Project (*.wproj)\0*.wproj\0");
	if (filepath.empty())
		return false;
	return OpenProject(filepath);
}

bool EditorProjectManager::OpenProject(const std::filesystem::path& path)
{
	WHP_PROFILE_FUNCTION();
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
		RepairLoadedProjectScaffold(Project::GetActive());
		WHP_EDITOR_INFO("[Project] Project scaffold verified.");
		MigrateProjectNativeFileExtensions();
		WHP_EDITOR_INFO("[Project] Native file extension migration complete.");
		const bool scriptBuildSucceeded = layer.m_ScriptManager.BuildProjectScripts();
		if (!scriptBuildSucceeded)
			WHP_EDITOR_WARN("[Script Build] Project opened, but script build failed.");
		WHP_EDITOR_INFO("[Project] Script build step complete.");
		ScriptEngine::Init();
		WHP_EDITOR_INFO("[Project] Script engine initialized.");
		layer.m_ScriptManager.StartSourceWatcher();
		const AssetHandle configuredStartScene = Project::GetActive()->GetConfig().m_StartScene;
		const AssetHandle startScene = ResolveStartSceneHandle(Project::GetActive());
		if (startScene != configuredStartScene)
		{
			if (configuredStartScene != 0 && startScene != 0)
				WHP_EDITOR_WARN("[Project] Configured start scene is invalid. Falling back to first available scene.");
			else if (configuredStartScene != 0)
				WHP_EDITOR_WARN("[Project] Configured start scene is invalid. Resetting Project start scene.");

			Project::GetActive()->GetConfig().m_StartScene = startScene;
			Project::SaveActive();
		}

		if (startScene)
		{
			layer.m_SceneManager.OpenScene(startScene);
			if (!layer.m_SceneManager.EditorScene())
			{
				WHP_EDITOR_WARN("[Project] Start scene could not be opened. Opening an empty scene.");
				layer.m_SceneManager.NewScene();
			}
		}
		else
		{
			if (configuredStartScene != 0)
			{
				Project::GetActive()->GetConfig().m_StartScene = 0;
				Project::SaveActive();
			}
			layer.m_SceneManager.NewScene();
		}
		layer.m_ContentBrowserPanel = MakeScope<ContentBrowserPanel>(Project::GetActive());
		layer.m_ContentBrowserPanel->SetAssetOpenCallback([this](AssetHandle handle) { return GetLayer().m_AssetInteractionManager.HandleContentBrowserAssetOpen(handle); });
		layer.m_ContentBrowserPanel->SetAssetInspectCallback([this](AssetHandle handle) { return GetLayer().m_AssetInteractionManager.HandleContentBrowserAssetInspect(handle); });
		ApplyPreferencesToContentBrowser();
		layer.RegisterEditorShortcuts();
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
	WHP_PROFILE_FUNCTION();
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
	layer.RegisterEditorShortcuts();
}

_WHIP_END
