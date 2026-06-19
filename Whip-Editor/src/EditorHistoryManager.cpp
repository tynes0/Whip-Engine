#include <Whip-Editor/EditorHistoryManager.h>

#include <Whip-Editor/EditorLayer.h>

#include <Whip/Scene/SceneSerializer.h>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <unordered_set>

_WHIP_START

namespace
{
	std::string ReadTextFile(const std::filesystem::path& path)
	{
		std::ifstream stream(path, std::ios::binary);
		if (!stream)
			return {};

		return std::string{ std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() };
	}

	bool WriteTextFile(const std::filesystem::path& path, const std::string& contents)
	{
		std::error_code error;
		std::filesystem::create_directories(path.parent_path(), error);
		std::ofstream stream(path, std::ios::binary | std::ios::trunc);
		if (!stream)
			return false;

		stream << contents;
		return true;
	}
}

EditorHistoryManager::ProjectHistoryEntry EditorHistoryManager::CaptureProjectHistory(const EditorLayer& layer) const
{
	ProjectHistoryEntry entry;
	Ref<Project> activeProject = Project::GetActive();
	if (!activeProject || !activeProject->GetEditorAssetManager())
		return entry;

	entry.m_Valid = true;
	entry.m_Config = activeProject->GetConfig();
	entry.m_ProjectPath = activeProject->GetProjectPath();
	entry.m_AssetRegistryPath = activeProject->GetAssetRegistryPath();
	entry.m_ProjectFileContents = ReadTextFile(entry.m_ProjectPath);
	entry.m_AssetRegistryContents = ReadTextFile(entry.m_AssetRegistryPath);

	const AssetRegistry& registry = activeProject->GetEditorAssetManager()->GetAssetRegistry();
	registry.Foreach(AssetType::Scene, [activeProject, &entry](const AssetRegistry::ValueType& value)
		{
			const std::string relativePath = value.second.m_Filepath.generic_string();
			entry.m_SceneFileContents[relativePath] = ReadTextFile(activeProject->GetAssetDirectory() / value.second.m_Filepath);
		});

	return entry;
}

void EditorHistoryManager::RestoreProjectHistory(EditorLayer& layer, const ProjectHistoryEntry& entry)
{
	if (!entry.m_Valid)
		return;

	Ref<Project> activeProject = Project::GetActive();
	if (!activeProject || !activeProject->GetEditorAssetManager())
		return;
	if (!entry.m_ProjectPath.empty() && activeProject->GetProjectPath() != entry.m_ProjectPath)
		return;

	std::unordered_set<std::string> currentScenePaths;
	const std::filesystem::path currentAssetDirectory = activeProject->GetAssetDirectory();
	activeProject->GetEditorAssetManager()->GetAssetRegistry().Foreach(AssetType::Scene, [&currentScenePaths](const AssetRegistry::ValueType& value)
		{
			currentScenePaths.insert(value.second.m_Filepath.generic_string());
		});

	activeProject->GetConfig() = entry.m_Config;
	if (!entry.m_ProjectFileContents.empty())
		WriteTextFile(entry.m_ProjectPath, entry.m_ProjectFileContents);
	else
		Project::SaveActive();

	const std::filesystem::path restoredAssetDirectory = entry.m_ProjectPath.parent_path() / entry.m_Config.m_AssetDirectory;
	const std::filesystem::path restoredAssetRegistryPath = restoredAssetDirectory / entry.m_Config.m_AssetRegistryPath;
	if (!entry.m_AssetRegistryContents.empty())
		WriteTextFile(restoredAssetRegistryPath, entry.m_AssetRegistryContents);

	for (const auto& [relativePath, contents] : entry.m_SceneFileContents)
		WriteTextFile(restoredAssetDirectory / relativePath, contents);

	for (const std::string& relativePath : currentScenePaths)
	{
		if (entry.m_SceneFileContents.find(relativePath) != entry.m_SceneFileContents.end())
			continue;

		std::error_code error;
		std::filesystem::remove(currentAssetDirectory / relativePath, error);
		if (currentAssetDirectory != restoredAssetDirectory)
			std::filesystem::remove(restoredAssetDirectory / relativePath, error);
	}

	activeProject->GetEditorAssetManager()->DeserializeAssetRegistry();
	if (layer.m_ContentBrowserPanel)
	{
		layer.m_ContentBrowserPanel = MakeScope<ContentBrowserPanel>(activeProject);
		layer.m_ContentBrowserPanel->SetAssetOpenCallback([&layer](AssetHandle handle) { return layer.m_AssetInteractionManager.HandleContentBrowserAssetOpen(layer, handle); });
		layer.m_ContentBrowserPanel->SetAssetInspectCallback([&layer](AssetHandle handle) { return layer.m_AssetInteractionManager.HandleContentBrowserAssetInspect(layer, handle); });
		layer.m_ProjectManager.ApplyPreferencesToContentBrowser(layer);
	}
}

void EditorHistoryManager::CaptureSceneHistory(EditorLayer& layer, bool includeProjectSnapshot)
{
	if (layer.m_SceneState != EditorSceneState::Edit || !layer.m_EditorScene)
		return;

	SceneHistoryEntry entry;
	entry.m_SceneSnapshot = Scene::Copy(layer.m_EditorScene);
	entry.m_EditorScenePath = layer.m_EditorScenePath;
	entry.m_SelectedEntities = layer.m_SceneHierarchyPanel.GetSelectedEntityIds();
	if (includeProjectSnapshot)
		entry.m_ProjectSnapshot = CaptureProjectHistory(layer);
	m_UndoStack.push_back(entry);
	m_RedoStack.clear();
	layer.MarkSceneDirty();

	static constexpr size_t maxHistoryEntries = 64;
	if (m_UndoStack.size() > maxHistoryEntries)
		m_UndoStack.erase(m_UndoStack.begin());
}

void EditorHistoryManager::RestoreSceneHistory(EditorLayer& layer, const SceneHistoryEntry& entry)
{
	if (!entry.m_SceneSnapshot)
		return;

	if (layer.m_SceneState != EditorSceneState::Edit)
		layer.OnSceneStop();

	RestoreProjectHistory(layer, entry.m_ProjectSnapshot);
	layer.m_EditorScene = Scene::Copy(entry.m_SceneSnapshot);
	layer.m_EditorScenePath = entry.m_EditorScenePath;
	layer.m_EditorScene->OnViewportResize((uint32_t)layer.m_ViewportSize.x, (uint32_t)layer.m_ViewportSize.y);
	layer.m_ActiveScene = layer.m_EditorScene;
	layer.m_SceneHierarchyPanel.SetContext(layer.m_EditorScene);
	layer.m_SceneHierarchyPanel.SetSelectedEntityIds(entry.m_SelectedEntities);
}

void EditorHistoryManager::UndoScene(EditorLayer& layer)
{
	if (m_UndoStack.empty() || layer.m_SceneState != EditorSceneState::Edit)
		return;

	SceneHistoryEntry current;
	current.m_SceneSnapshot = Scene::Copy(layer.m_EditorScene);
	current.m_EditorScenePath = layer.m_EditorScenePath;
	current.m_SelectedEntities = layer.m_SceneHierarchyPanel.GetSelectedEntityIds();
	SceneHistoryEntry entry = m_UndoStack.back();
	if (entry.m_ProjectSnapshot.m_Valid)
		current.m_ProjectSnapshot = CaptureProjectHistory(layer);
	m_RedoStack.push_back(current);

	m_UndoStack.pop_back();
	RestoreSceneHistory(layer, entry);
	layer.MarkSceneDirty();
}

void EditorHistoryManager::RedoScene(EditorLayer& layer)
{
	if (m_RedoStack.empty() || layer.m_SceneState != EditorSceneState::Edit)
		return;

	SceneHistoryEntry current;
	current.m_SceneSnapshot = Scene::Copy(layer.m_EditorScene);
	current.m_EditorScenePath = layer.m_EditorScenePath;
	current.m_SelectedEntities = layer.m_SceneHierarchyPanel.GetSelectedEntityIds();
	SceneHistoryEntry entry = m_RedoStack.back();
	if (entry.m_ProjectSnapshot.m_Valid)
		current.m_ProjectSnapshot = CaptureProjectHistory(layer);
	m_UndoStack.push_back(current);

	m_RedoStack.pop_back();
	RestoreSceneHistory(layer, entry);
	layer.MarkSceneDirty();
}

void EditorHistoryManager::ClearSceneHistory()
{
	m_UndoStack.clear();
	m_RedoStack.clear();
	m_GizmoHistoryActive = false;
}

void EditorHistoryManager::DuplicateSelection(EditorLayer& layer)
{
	if (layer.m_SceneState != EditorSceneState::Edit)
		return;

	std::vector<Entity> selectedEntities = layer.m_SceneHierarchyPanel.GetSelectedEntities();
	if (selectedEntities.empty())
		return;

	CaptureSceneHistory(layer);
	bool append = false;
	for (Entity selectedEntity : selectedEntities)
	{
		Entity duplicated = layer.m_EditorScene->DuplicateEntity(selectedEntity);
		layer.m_SceneHierarchyPanel.SetSelectedEntity(duplicated, append);
		append = true;
	}
}

void EditorHistoryManager::DeleteSelection(EditorLayer& layer)
{
	if (Application::Get().GetImGuiLayer()->GetActiveWidgetID() != 0)
		return;

	std::vector<Entity> selectedEntities = layer.m_SceneHierarchyPanel.GetSelectedEntities();
	if (selectedEntities.empty())
		return;

	CaptureSceneHistory(layer);
	std::vector<UUID> selectedIds;
	selectedIds.reserve(selectedEntities.size());
	for (Entity selectedEntity : selectedEntities)
		selectedIds.push_back(selectedEntity.GetUUID());

	auto hasSelectedAncestor = [&](Entity selectedEntity)
		{
			while (selectedEntity && selectedEntity.HasComponent<HierarchyComponent>())
			{
				UUID parentId = selectedEntity.GetComponent<HierarchyComponent>().m_Parent;
				if (parentId == 0)
					return false;
				if (std::find(selectedIds.begin(), selectedIds.end(), parentId) != selectedIds.end())
					return true;
				selectedEntity = layer.m_ActiveScene->FindEntityByUUID(parentId);
			}
			return false;
		};

	layer.m_SceneHierarchyPanel.ClearSelection();
	for (Entity selectedEntity : selectedEntities)
		if (selectedEntity && !hasSelectedAncestor(selectedEntity))
			layer.m_ActiveScene->DestroyEntity(selectedEntity);
}

void EditorHistoryManager::SelectAll(EditorLayer& layer)
{
	if (layer.m_SceneState == EditorSceneState::Edit)
		layer.m_SceneHierarchyPanel.SelectAll();
}

void EditorHistoryManager::CopySelection(EditorLayer& layer)
{
	m_EntityClipboard = layer.m_SceneHierarchyPanel.GetSelectedEntityIds();
}

void EditorHistoryManager::PasteSelection(EditorLayer& layer)
{
	if (layer.m_SceneState != EditorSceneState::Edit || m_EntityClipboard.empty())
		return;

	std::vector<Entity> sourceEntities;
	for (UUID id : m_EntityClipboard)
	{
		Entity source = layer.m_EditorScene->FindEntityByUUID(id);
		if (source)
			sourceEntities.push_back(source);
	}

	if (sourceEntities.empty())
		return;

	CaptureSceneHistory(layer);
	bool append = false;
	for (Entity source : sourceEntities)
	{
		Entity pasted = layer.m_EditorScene->DuplicateEntity(source);
		layer.m_SceneHierarchyPanel.SetSelectedEntity(pasted, append);
		append = true;
	}
}

void EditorHistoryManager::CutSelection(EditorLayer& layer)
{
	CopySelection(layer);
	DeleteSelection(layer);
}

_WHIP_END
