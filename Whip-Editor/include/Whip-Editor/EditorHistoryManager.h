#pragma once

#include <Whip.h>

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

_WHIP_START

class EditorLayer;

class EditorHistoryManager
{
public:
	explicit EditorHistoryManager(EditorLayer* boundedLayer = nullptr);
	~EditorHistoryManager();

	struct ProjectHistoryEntry
	{
		bool m_Valid = false;
		ProjectConfig m_Config;
		std::filesystem::path m_ProjectPath;
		std::filesystem::path m_AssetRegistryPath;
		std::string m_ProjectFileContents;
		std::string m_AssetRegistryContents;
		std::unordered_map<std::string, std::string> m_SceneFileContents;
	};

	struct SceneHistoryEntry
	{
		Ref<Scene> m_SceneSnapshot;
		std::filesystem::path m_EditorScenePath;
		std::vector<UUID> m_SelectedEntities;
		ProjectHistoryEntry m_ProjectSnapshot;
	};

	bool CanUndo() const { return !m_UndoStack.empty(); }
	bool CanRedo() const { return !m_RedoStack.empty(); }
	bool HasClipboard() const { return !m_EntityClipboard.empty(); }

	bool IsGizmoHistoryActive() const { return m_GizmoHistoryActive; }
	void SetGizmoHistoryActive(bool active) { m_GizmoHistoryActive = active; }

	void Bind(EditorLayer& layer);

	ProjectHistoryEntry CaptureProjectHistory() const;
	void RestoreProjectHistory(const ProjectHistoryEntry& entry);

	void CaptureSceneHistory(bool includeProjectSnapshot = false);
	void RestoreSceneHistory(const SceneHistoryEntry& entry);
	void UndoScene();
	void RedoScene();
	void ClearSceneHistory();

	void DuplicateSelection();
	void DeleteSelection();
	void SelectAll();
	void CopySelection();
	void PasteSelection();
	void CutSelection();

private:
	EditorLayer& GetLayer() const;

	EditorLayer* m_BoundedLayer = nullptr;
	std::vector<SceneHistoryEntry> m_UndoStack;
	std::vector<SceneHistoryEntry> m_RedoStack;
	std::vector<UUID> m_EntityClipboard;
	bool m_GizmoHistoryActive = false;
};

_WHIP_END
