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
	explicit EditorSceneManager(EditorLayer* boundedLayer = nullptr);
	~EditorSceneManager();

	const Ref<Scene>& ActiveScene() const;

	const Ref<Scene>& EditorScene() const;

	const std::filesystem::path& EditorScenePath() const;

	EditorSceneState State() const;

	bool IsSceneDirty() const;

	const std::chrono::steady_clock::time_point& LastRecoverySnapshot() const;

	void Bind(EditorLayer& layer);
	void SetActiveScene(Ref<Scene> scene);
	void SetEditorScene(Ref<Scene> scene);
	void SetEditorScenePath(std::filesystem::path path);

	void ResetToEmptyScene();

	void NewScene();
	void OpenScene(AssetHandle handle);
	void CloseScene();
	void SaveScene();
	void SaveSceneAs();
	void MarkDirty();
	void MarkClean();
	std::filesystem::path GetRecoveryPath() const;
	void WriteRecoverySnapshot(const char* reason);

	void ProcessRuntimeSceneTransition();
	bool LoadRuntimeScene(AssetHandle handle);
	bool UnloadRuntimeScene();
	void StopActiveRuntimeSceneForTransition();
	void StartActiveRuntimeSceneForTransition(AssetHandle handle);

	void SerializeScene(Ref<Scene> sceneIn, const std::filesystem::path& path);

	void OnScenePlay();
	void OnSceneSimulate();
	void OnSceneStop();
	void OnScenePause();

private:
	EditorLayer& GetLayer() const;

	EditorLayer* m_BoundedLayer;

	Ref<Scene> m_ActiveScene;
	Ref<Scene> m_EditorScene;
	std::filesystem::path m_EditorScenePath;
	bool m_SceneDirty;
	std::chrono::steady_clock::time_point m_LastRecoverySnapshot;
	EditorSceneState m_State;
};

_WHIP_END
