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

	std::vector<SceneHistoryEntry>& GetUndoStackStorage() { return m_UndoStack; }
	std::vector<SceneHistoryEntry>& GetRedoStackStorage() { return m_RedoStack; }
	std::vector<UUID>& GetEntityClipboardStorage() { return m_EntityClipboard; }
	bool& GetGizmoHistoryActiveStorage() { return m_GizmoHistoryActive; }

	ProjectHistoryEntry CaptureProjectHistory(const EditorLayer& layer) const;
	void RestoreProjectHistory(EditorLayer& layer, const ProjectHistoryEntry& entry);

	void CaptureSceneHistory(EditorLayer& layer, bool includeProjectSnapshot = false);
	void RestoreSceneHistory(EditorLayer& layer, const SceneHistoryEntry& entry);
	void UndoScene(EditorLayer& layer);
	void RedoScene(EditorLayer& layer);
	void ClearSceneHistory();

	void DuplicateSelection(EditorLayer& layer);
	void DeleteSelection(EditorLayer& layer);
	void SelectAll(EditorLayer& layer);
	void CopySelection(EditorLayer& layer);
	void PasteSelection(EditorLayer& layer);
	void CutSelection(EditorLayer& layer);

private:
	std::vector<SceneHistoryEntry> m_UndoStack;
	std::vector<SceneHistoryEntry> m_RedoStack;
	std::vector<UUID> m_EntityClipboard;
	bool m_GizmoHistoryActive = false;
};

_WHIP_END
