#include <Whip-Editor/EditorSceneManager.h>

#include <Whip-Editor/EditorLayer.h>

#include <Whip/Scripting/ScriptEngine.h>
#include <Whip/Utils/FileExtensions.h>

_WHIP_START

void EditorSceneManager::ResetToEmptyScene()
{
	m_EditorScene = MakeRef<Scene>();
	m_ActiveScene = m_EditorScene;
	m_EditorScenePath.clear();
	m_SceneDirty = false;
	m_LastRecoverySnapshot = {};
	m_State = EditorSceneState::Edit;
}

void EditorSceneManager::NewScene(EditorLayer& layer) { layer.NewScene(); }
void EditorSceneManager::OpenScene(EditorLayer& layer, AssetHandle handle) { layer.OpenScene(handle); }
void EditorSceneManager::CloseScene(EditorLayer& layer) { layer.CloseScene(); }
void EditorSceneManager::SaveScene(EditorLayer& layer) { layer.SaveScene(); }
void EditorSceneManager::SaveSceneAs(EditorLayer& layer) { layer.SaveSceneAs(); }
void EditorSceneManager::MarkDirty() { m_SceneDirty = true; }
void EditorSceneManager::MarkClean() { m_SceneDirty = false; }
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
void EditorSceneManager::WriteRecoverySnapshot(EditorLayer& layer, const char* reason) { layer.WriteSceneRecoverySnapshot(reason); }
void EditorSceneManager::ProcessRuntimeSceneTransition(EditorLayer& layer) { layer.ProcessRuntimeSceneTransition(); }
bool EditorSceneManager::LoadRuntimeScene(EditorLayer& layer, AssetHandle handle) { return layer.LoadRuntimeScene(handle); }
bool EditorSceneManager::UnloadRuntimeScene(EditorLayer& layer) { return layer.UnloadRuntimeScene(); }
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
void EditorSceneManager::SerializeScene(EditorLayer& layer, Ref<Scene> sceneIn, const std::filesystem::path& path) { layer.SerializeScene(sceneIn, path); }
void EditorSceneManager::OnScenePlay(EditorLayer& layer) { layer.OnScenePlay(); }
void EditorSceneManager::OnSceneSimulate(EditorLayer& layer) { layer.OnSceneSimulate(); }
void EditorSceneManager::OnSceneStop(EditorLayer& layer) { layer.OnSceneStop(); }
void EditorSceneManager::OnScenePause(EditorLayer& layer) { layer.OnScenePause(); }

_WHIP_END
