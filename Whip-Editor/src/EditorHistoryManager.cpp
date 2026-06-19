#include <Whip-Editor/EditorHistoryManager.h>

#include <Whip-Editor/EditorLayer.h>

_WHIP_START

EditorHistoryManager::ProjectHistoryEntry EditorHistoryManager::CaptureProjectHistory(const EditorLayer& layer) const
{
	return layer.CaptureProjectHistory();
}

void EditorHistoryManager::RestoreProjectHistory(EditorLayer& layer, const ProjectHistoryEntry& entry) { layer.RestoreProjectHistory(entry); }
void EditorHistoryManager::CaptureSceneHistory(EditorLayer& layer, bool includeProjectSnapshot) { layer.CaptureSceneHistory(includeProjectSnapshot); }
void EditorHistoryManager::RestoreSceneHistory(EditorLayer& layer, const SceneHistoryEntry& entry) { layer.RestoreSceneHistory(entry); }
void EditorHistoryManager::UndoScene(EditorLayer& layer) { layer.UndoScene(); }
void EditorHistoryManager::RedoScene(EditorLayer& layer) { layer.RedoScene(); }
void EditorHistoryManager::ClearSceneHistory()
{
	m_UndoStack.clear();
	m_RedoStack.clear();
	m_GizmoHistoryActive = false;
}

void EditorHistoryManager::DuplicateSelection(EditorLayer& layer) { layer.OnDuplicatedEntity(); }
void EditorHistoryManager::DeleteSelection(EditorLayer& layer) { layer.OnDeletedEntity(); }
void EditorHistoryManager::SelectAll(EditorLayer& layer) { layer.OnSelectAllEntities(); }
void EditorHistoryManager::CopySelection(EditorLayer& layer) { layer.OnCopyEntities(); }
void EditorHistoryManager::PasteSelection(EditorLayer& layer) { layer.OnPasteEntities(); }
void EditorHistoryManager::CutSelection(EditorLayer& layer) { layer.OnCutEntities(); }

_WHIP_END
