#pragma once

#include <Whip.h>

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include "EditorManagerBase.h"

_WHIP_START

class EditorHistoryManager : public EditorManagerBase // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	explicit EditorHistoryManager(EditorLayer* boundedLayer = nullptr);
	~EditorHistoryManager() override;

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

	bool CanUndo() const;
	bool CanRedo() const;
	bool HasClipboard() const;

	bool IsGizmoHistoryActive() const;
	void SetGizmoHistoryActive(bool active);

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
	std::vector<SceneHistoryEntry> m_UndoStack;
	std::vector<SceneHistoryEntry> m_RedoStack;
	std::vector<UUID> m_EntityClipboard;
	bool m_GizmoHistoryActive = false;
};

_WHIP_END
