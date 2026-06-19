#include <Whip-Editor/EditorSceneManager.h>

#include <Whip-Editor/EditorLayer.h>

#include <Whip/Asset/AssetManager.h>
#include <Whip/Asset/SceneImporter.h>
#include <Whip/Scripting/ScriptEngine.h>
#include <Whip/Utils/FileExtensions.h>
#include <Whip/Utils/PlatformUtils.h>

#include <fstream>
#include <utility>

_WHIP_START

namespace
{
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

EditorSceneManager::EditorSceneManager(EditorLayer* boundedLayer)
	: m_BoundedLayer(boundedLayer), m_SceneDirty(false), m_State(EditorSceneState::Edit)
{
}

EditorSceneManager::~EditorSceneManager() = default;

void EditorSceneManager::Bind(EditorLayer& layer)
{
	m_BoundedLayer = &layer;
}

EditorLayer& EditorSceneManager::GetLayer() const
{
	WHP_CORE_ASSERT(m_BoundedLayer, "EditorSceneManager is not bound to an EditorLayer.");
	return *m_BoundedLayer;
}

const Ref<Scene>& EditorSceneManager::ActiveScene() const
{
	return m_ActiveScene;
}

const Ref<Scene>& EditorSceneManager::EditorScene() const
{
	return m_EditorScene;
}

const std::filesystem::path& EditorSceneManager::EditorScenePath() const
{
	return m_EditorScenePath;
}

EditorSceneState EditorSceneManager::State() const
{
	return m_State;
}

bool EditorSceneManager::IsSceneDirty() const
{
	return m_SceneDirty;
}

const std::chrono::steady_clock::time_point& EditorSceneManager::LastRecoverySnapshot() const
{
	return m_LastRecoverySnapshot;
}

void EditorSceneManager::SetActiveScene(Ref<Scene> scene)
{
	m_ActiveScene = std::move(scene);
}

void EditorSceneManager::SetEditorScene(Ref<Scene> scene)
{
	m_EditorScene = std::move(scene);
}

void EditorSceneManager::SetEditorScenePath(std::filesystem::path path)
{
	m_EditorScenePath = std::move(path);
}

void EditorSceneManager::ResetToEmptyScene()
{
	m_EditorScene = MakeRef<Scene>();
	m_ActiveScene = m_EditorScene;
	m_EditorScenePath.clear();
	m_SceneDirty = false;
	m_LastRecoverySnapshot = {};
	m_State = EditorSceneState::Edit;
}

void EditorSceneManager::NewScene()
{
	EditorLayer& layer = GetLayer();
	if (!layer.HasProjectLoaded())
		return;

	m_EditorScene = MakeRef<Scene>();
	m_ActiveScene = m_EditorScene;
	m_EditorScene->OnViewportResize((uint32_t)layer.m_ViewportSize.x, (uint32_t)layer.m_ViewportSize.y);
	layer.m_SceneHierarchyPanel.SetContext(m_EditorScene);
	m_EditorScenePath = std::filesystem::path();
	layer.m_HistoryManager.ClearSceneHistory();
	MarkClean();
}

void EditorSceneManager::OpenScene(AssetHandle handle)
{
	EditorLayer& layer = GetLayer();
	if (!layer.HasProjectLoaded())
		return;

	if (m_State != EditorSceneState::Edit)
		OnSceneStop();

	if (!Project::GetActive()->GetEditorAssetManager()->IsAssetHandleValid(handle))
	{
		WHP_EDITOR_WARN("[Scene] Failed to open scene. Asset handle is not registered.");
		return;
	}
	const std::filesystem::path scenePath = Project::GetActive()->GetEditorAssetManager()->GetFilepath(handle);
	if (!std::filesystem::exists(Project::GetActiveAssetDirectory() / scenePath))
	{
		WHP_EDITOR_WARN(std::string("[Scene] Failed to open scene. File is missing: ") + scenePath.string());
		return;
	}

	Ref<Scene> readOnlyScene = AssetManager::GetAsset<Scene>(handle);
	if (!readOnlyScene)
	{
		WHP_EDITOR_WARN("[Scene] Failed to open scene. Asset is missing or failed to import.");
		return;
	}
	Ref<Scene> newScene = Scene::Copy(readOnlyScene);

	m_EditorScene = newScene;
	layer.m_SceneHierarchyPanel.SetContext(m_EditorScene);

	m_ActiveScene = m_EditorScene;
	m_EditorScenePath = scenePath;
	layer.m_HistoryManager.ClearSceneHistory();
	MarkClean();
}

void EditorSceneManager::CloseScene()
{
	EditorLayer& layer = GetLayer();
	if (m_State != EditorSceneState::Edit)
		OnSceneStop();
	Ref<Scene> newScene = MakeRef<Scene>();
	m_EditorScene = newScene;
	m_EditorScene->OnViewportResize((uint32_t)layer.m_ViewportSize.x, (uint32_t)layer.m_ViewportSize.y);
	m_ActiveScene = m_EditorScene;
	m_EditorScenePath.clear();
	layer.m_SceneHierarchyPanel.SetContext({});
	layer.m_HistoryManager.ClearSceneHistory();
	MarkClean();
}

void EditorSceneManager::SaveScene()
{
	if (!GetLayer().HasProjectLoaded())
		return;

	if (!m_EditorScenePath.empty())
	{
		SerializeScene(m_ActiveScene, m_EditorScenePath);
		MarkClean();
	}
	else
		SaveSceneAs();
}

void EditorSceneManager::SaveSceneAs()
{
	EditorLayer& layer = GetLayer();
	if (!layer.HasProjectLoaded())
		return;

	const std::filesystem::path scenesDirectory = Project::GetActiveAssetDirectory() / "Scenes";
	std::error_code error;
	std::filesystem::create_directories(scenesDirectory, error);

	std::string filepath = FileDialogs::SaveFile("Whip Scene (*.wscene)\0*.wscene\0", scenesDirectory.string().c_str());
	if (filepath.empty())
		return;

	std::filesystem::path scenePath(filepath);
	if (!FileExtensions::IsSceneExtension(scenePath))
		scenePath.replace_extension(FileExtensions::Scene);
	else if (FileExtensions::IsLegacySceneExtension(scenePath))
		scenePath.replace_extension(FileExtensions::Scene);

	SerializeScene(m_ActiveScene, scenePath);

	const std::filesystem::path assetDirectory = Project::GetActiveAssetDirectory();
	if (PathIsOrIsUnder(scenePath, assetDirectory))
	{
		error.clear();
		const std::filesystem::path relativePath = std::filesystem::relative(scenePath, assetDirectory, error).lexically_normal();
		if (!error)
		{
			m_EditorScenePath = relativePath;
			AssetHandle handle = Project::GetActive()->GetEditorAssetManager()->GetHandleFromFilepath(relativePath);
			if (handle == 0)
				handle = Project::GetActive()->GetEditorAssetManager()->ImportAsset(relativePath);
			if (handle != 0)
			{
				m_ActiveScene->m_Handle = handle;
				if (m_EditorScene)
					m_EditorScene->m_Handle = handle;
			}
		}
		else
		{
			m_EditorScenePath = scenePath;
		}
	}
	else
	{
		m_EditorScenePath = scenePath;
	}

	if (layer.m_ContentBrowserPanel)
		layer.m_ContentBrowserPanel->RefreshAssetTree();
	MarkClean();
}

void EditorSceneManager::MarkDirty()
{
	m_SceneDirty = true;
}
void EditorSceneManager::MarkClean()
{
	m_SceneDirty = false;
}

std::filesystem::path EditorSceneManager::GetRecoveryPath() const
{
	Ref<Project> activeProject = Project::GetActive();
	if (!activeProject)
		return {};

	std::filesystem::path recoveryDirectory = activeProject->GetProjectDirectory() / ".whip_recovery";
	std::string sceneName = m_EditorScenePath.empty() ? "Untitled" : m_EditorScenePath.filename().stem().string();
	if (sceneName.empty())
		sceneName = "Untitled";

	return recoveryDirectory / (sceneName + ".recovery" + FileExtensions::Scene);
}

void EditorSceneManager::WriteRecoverySnapshot(const char* reason)
{
	if (!GetLayer().HasProjectLoaded() || !m_EditorScene || !m_SceneDirty || m_State != EditorSceneState::Edit)
		return;

	const std::filesystem::path recoveryPath = GetRecoveryPath();
	if (recoveryPath.empty())
		return;

	std::error_code error;
	std::filesystem::create_directories(recoveryPath.parent_path(), error);
	if (error)
	{
		WHP_EDITOR_WARN(std::string("[Scene Recovery] Could not create recovery directory: ") + error.message());
		return;
	}

	SceneImporter::SaveScene(m_EditorScene, recoveryPath);
	m_LastRecoverySnapshot = std::chrono::steady_clock::now();
	WHP_EDITOR_INFO(std::string("[Scene Recovery] Snapshot written (") + reason + "): " + recoveryPath.string());
}

void EditorSceneManager::ProcessRuntimeSceneTransition()
{
	if (m_State != EditorSceneState::Play && m_State != EditorSceneState::Simulate)
	{
		ScriptEngine::ClearRuntimeSceneTransitionRequest();
		return;
	}

	const RuntimeSceneTransitionRequest request = ScriptEngine::ConsumeRuntimeSceneTransitionRequest();
	switch (request.m_Type)
	{
	case RuntimeSceneTransitionType::Load:
	case RuntimeSceneTransitionType::Reload:
		LoadRuntimeScene(request.m_SceneHandle);
		break;
	case RuntimeSceneTransitionType::Unload:
		UnloadRuntimeScene();
		break;
	default:
		break;
	}
}

bool EditorSceneManager::LoadRuntimeScene(AssetHandle handle)
{
	EditorLayer& layer = GetLayer();
	if (!layer.HasProjectLoaded() || handle == 0)
		return false;

	Ref<Project> activeProject = Project::GetActive();
	if (!activeProject->GetRuntimeAssetManager()->IsAssetHandleValid(handle) ||
		activeProject->GetRuntimeAssetManager()->GetAssetType(handle) != AssetType::Scene)
	{
		WHP_EDITOR_WARN("[Scene Manager] Runtime scene load failed. Invalid scene handle.");
		return false;
	}

	Ref<Scene> sourceScene = AssetManager::GetAsset<Scene>(handle);
	if (!sourceScene)
	{
		WHP_EDITOR_WARN("[Scene Manager] Runtime scene load failed. Scene Asset could not be loaded.");
		return false;
	}

	StopActiveRuntimeSceneForTransition();
	m_ActiveScene = Scene::Copy(sourceScene);
	m_ActiveScene->OnViewportResize(static_cast<uint32_t>(layer.m_ViewportSize.x), static_cast<uint32_t>(layer.m_ViewportSize.y));
	layer.m_SceneHierarchyPanel.SetContext(m_ActiveScene);
	layer.m_SceneHierarchyPanel.SetSelectedEntity({});
	StartActiveRuntimeSceneForTransition(handle);
	WHP_EDITOR_INFO(std::string("[Scene Manager] Runtime scene loaded: ") + activeProject->GetRuntimeAssetManager()->GetFilepath(handle).generic_string());
	return true;
}

bool EditorSceneManager::UnloadRuntimeScene()
{
	EditorLayer& layer = GetLayer();
	if (m_State != EditorSceneState::Play && m_State != EditorSceneState::Simulate)
		return false;

	StopActiveRuntimeSceneForTransition();
	m_ActiveScene = MakeRef<Scene>();
	m_ActiveScene->OnViewportResize(static_cast<uint32_t>(layer.m_ViewportSize.x), static_cast<uint32_t>(layer.m_ViewportSize.y));
	layer.m_SceneHierarchyPanel.SetContext(m_ActiveScene);
	layer.m_SceneHierarchyPanel.SetSelectedEntity({});
	StartActiveRuntimeSceneForTransition(0);
	WHP_EDITOR_INFO("[Scene Manager] Runtime scene unloaded.");
	return true;
}

void EditorSceneManager::StopActiveRuntimeSceneForTransition()
{
	if (!m_ActiveScene)
		return;

	if (m_State == EditorSceneState::Simulate)
		m_ActiveScene->OnSimulationStop();
	else if (m_State == EditorSceneState::Play)
		m_ActiveScene->OnRuntimeStop();
}

void EditorSceneManager::StartActiveRuntimeSceneForTransition(AssetHandle handle)
{
	if (!m_ActiveScene)
		return;

	ScriptEngine::SetRuntimeActiveSceneHandle(handle);
	if (m_State == EditorSceneState::Simulate)
		m_ActiveScene->OnSimulationStart();
	else if (m_State == EditorSceneState::Play)
		m_ActiveScene->OnRuntimeStart();
	ScriptEngine::SetRuntimeActiveSceneHandle(handle);
}

void EditorSceneManager::SerializeScene(Ref<Scene> sceneIn, const std::filesystem::path& path)
{
	SceneImporter::SaveScene(sceneIn, path);

	if (!GetLayer().HasProjectLoaded() || !sceneIn)
		return;

	const std::filesystem::path assetDirectory = Project::GetActiveAssetDirectory();
	std::filesystem::path scenePath = path;
	if (!scenePath.is_absolute())
		scenePath = assetDirectory / scenePath;
	scenePath = scenePath.lexically_normal();

	if (!PathIsOrIsUnder(scenePath, assetDirectory))
		return;

	std::error_code error;
	const std::filesystem::path relativePath = std::filesystem::relative(scenePath, assetDirectory, error).lexically_normal();
	if (error || relativePath.empty() || !FileExtensions::IsSceneExtension(relativePath))
		return;

	Ref<EditorAssetManager> editorAssetManager = Project::GetActive()->GetEditorAssetManager();
	if (!editorAssetManager)
		return;

	AssetHandle handle = editorAssetManager->GetHandleFromFilepath(relativePath);
	if (handle == 0)
		handle = editorAssetManager->ImportAsset(relativePath);

	if (handle == 0)
		return;

	sceneIn->m_Handle = handle;
	if (m_EditorScene)
		m_EditorScene->m_Handle = handle;
	if (m_ActiveScene)
		m_ActiveScene->m_Handle = handle;

	editorAssetManager->SetLoadedAsset(handle, Scene::Copy(sceneIn));
}

void EditorSceneManager::OnScenePlay()
{
	EditorLayer& layer = GetLayer();
	if (!layer.HasProjectLoaded())
		return;

	if (m_State == EditorSceneState::Simulate)
		OnSceneStop();
	WriteRecoverySnapshot("Before play");
	Project::RunState(true);
	m_State = EditorSceneState::Play;
	ScriptEngine::SetFilewatcherState(false);
	m_ActiveScene = Scene::Copy(m_EditorScene);
	ScriptEngine::SetRuntimeActiveSceneHandle(m_ActiveScene->m_Handle);
	m_ActiveScene->OnRuntimeStart();
	ScriptEngine::SetRuntimeActiveSceneHandle(m_ActiveScene->m_Handle);
	layer.m_LastSelectedEntity = layer.m_SceneHierarchyPanel.GetSelectedEntity();
	layer.m_SceneHierarchyPanel.SetContext(m_ActiveScene);
}

void EditorSceneManager::OnSceneSimulate()
{
	EditorLayer& layer = GetLayer();
	if (!layer.HasProjectLoaded())
		return;

	if (m_State == EditorSceneState::Play)
		OnSceneStop();

	WriteRecoverySnapshot("Before simulate");
	Project::RunState(true);
	m_State = EditorSceneState::Simulate;
	ScriptEngine::SetFilewatcherState(false);
	m_ActiveScene = Scene::Copy(m_EditorScene);
	ScriptEngine::SetRuntimeActiveSceneHandle(m_ActiveScene->m_Handle);
	m_ActiveScene->OnSimulationStart();
	ScriptEngine::SetRuntimeActiveSceneHandle(m_ActiveScene->m_Handle);
	layer.m_LastSelectedEntity = layer.m_SceneHierarchyPanel.GetSelectedEntity();
	layer.m_SceneHierarchyPanel.SetContext(m_ActiveScene);
	if (layer.m_LastSelectedEntity)
		layer.m_SceneHierarchyPanel.SetSelectedEntity(m_ActiveScene->FindEntityByUUID(layer.m_LastSelectedEntity.GetUUID()));
}

void EditorSceneManager::OnSceneStop()
{
	EditorLayer& layer = GetLayer();
	WHP_CORE_ASSERT(m_State == EditorSceneState::Play || m_State == EditorSceneState::Simulate, "invalid SceneState!");
	Project::RunState(false);
	if (m_State == EditorSceneState::Play)
		m_ActiveScene->OnRuntimeStop();
	else if (m_State == EditorSceneState::Simulate)
		m_ActiveScene->OnSimulationStop();
	m_State = EditorSceneState::Edit;
	ScriptEngine::ClearRuntimeSceneTransitionRequest();
	ScriptEngine::SetRuntimeActiveSceneHandle(0);
	ScriptEngine::SetFilewatcherState(true);
	m_ActiveScene = m_EditorScene;
	layer.m_SceneHierarchyPanel.SetContext(m_ActiveScene);
	layer.m_SceneHierarchyPanel.SetSelectedEntity(layer.m_LastSelectedEntity);
}

void EditorSceneManager::OnScenePause()
{
}

_WHIP_END
