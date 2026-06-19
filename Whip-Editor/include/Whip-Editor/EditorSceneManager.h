#pragma once

#include <Whip.h>

#include <chrono>
#include <filesystem>

_WHIP_START

class EditorLayer;

enum class EditorSceneState
{
	Edit = 0,
	Play = 1,
	Simulate = 2
};

class EditorSceneManager
{
public:
	Ref<Scene>& ActiveScene() { return m_ActiveScene; }
	const Ref<Scene>& ActiveScene() const { return m_ActiveScene; }

	Ref<Scene>& EditorScene() { return m_EditorScene; }
	const Ref<Scene>& EditorScene() const { return m_EditorScene; }

	std::filesystem::path& EditorScenePath() { return m_EditorScenePath; }
	const std::filesystem::path& EditorScenePath() const { return m_EditorScenePath; }

	EditorSceneState& State() { return m_State; }
	EditorSceneState State() const { return m_State; }

	bool& SceneDirty() { return m_SceneDirty; }
	bool IsSceneDirty() const { return m_SceneDirty; }

	std::chrono::steady_clock::time_point& LastRecoverySnapshot() { return m_LastRecoverySnapshot; }
	const std::chrono::steady_clock::time_point& LastRecoverySnapshot() const { return m_LastRecoverySnapshot; }

	Ref<Scene>& GetActiveSceneStorage() { return m_ActiveScene; }
	Ref<Scene>& GetEditorSceneStorage() { return m_EditorScene; }
	std::filesystem::path& GetEditorScenePathStorage() { return m_EditorScenePath; }
	bool& GetDirtyStorage() { return m_SceneDirty; }
	std::chrono::steady_clock::time_point& GetLastRecoverySnapshotStorage() { return m_LastRecoverySnapshot; }
	EditorSceneState& GetStateStorage() { return m_State; }

	void ResetToEmptyScene();

	void NewScene(EditorLayer& layer);
	void OpenScene(EditorLayer& layer, AssetHandle handle);
	void CloseScene(EditorLayer& layer);
	void SaveScene(EditorLayer& layer);
	void SaveSceneAs(EditorLayer& layer);
	void MarkDirty();
	void MarkClean();
	std::filesystem::path GetRecoveryPath() const;
	void WriteRecoverySnapshot(EditorLayer& layer, const char* reason);

	void ProcessRuntimeSceneTransition(EditorLayer& layer);
	bool LoadRuntimeScene(EditorLayer& layer, AssetHandle handle);
	bool UnloadRuntimeScene(EditorLayer& layer);
	void StopActiveRuntimeSceneForTransition();
	void StartActiveRuntimeSceneForTransition(AssetHandle handle);

	void SerializeScene(EditorLayer& layer, Ref<Scene> sceneIn, const std::filesystem::path& path);

	void OnScenePlay(EditorLayer& layer);
	void OnSceneSimulate(EditorLayer& layer);
	void OnSceneStop(EditorLayer& layer);
	void OnScenePause(EditorLayer& layer);

private:
	Ref<Scene> m_ActiveScene;
	Ref<Scene> m_EditorScene;
	std::filesystem::path m_EditorScenePath;
	bool m_SceneDirty = false;
	std::chrono::steady_clock::time_point m_LastRecoverySnapshot{};
	EditorSceneState m_State = EditorSceneState::Edit;
};

_WHIP_END
