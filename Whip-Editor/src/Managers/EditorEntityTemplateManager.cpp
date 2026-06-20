#include <Whip-Editor/Managers/EditorEntityTemplateManager.h>

#include <Whip-Editor/EditorLayer.h>

#include <Whip/Scene/SceneSerializer.h>

#include "Whip-Editor/Helpers/Utils.h"

_WHIP_START
	EditorEntityTemplateManager::EditorEntityTemplateManager(EditorLayer* boundedLayer)
	: EditorManagerBase(boundedLayer)
{
}

EditorEntityTemplateManager::~EditorEntityTemplateManager() = default;

void EditorEntityTemplateManager::SaveEntityTemplate(Entity entityIn)
{
	EditorLayer& layer = GetLayer();

	if (!layer.HasProjectLoaded() || !entityIn)
		return;

	const std::filesystem::path templatesDirectory = Project::GetActiveAssetDirectory() / "EntityTemplates";
	std::error_code error;
	std::filesystem::create_directories(templatesDirectory, error);

	std::string filepath = FileDialogs::SaveFile("Whip Entity Template (*.went)\0*.went\0", templatesDirectory.string().c_str());
	if (filepath.empty())
		return;

	std::filesystem::path templatePath(filepath);
	if (!FileExtensions::IsEntityTemplateExtension(templatePath))
		templatePath.replace_extension(FileExtensions::EntityTemplate);

	SceneSerializer serializer(layer.m_SceneManager.EditorScene());
	if (!serializer.SerializeEntityTemplate(entityIn, templatePath))
	{
		WHP_EDITOR_ERROR(std::string("[Entity Template] Could not save template: ") + templatePath.string());
		return;
	}

	const std::filesystem::path assetDirectory = Project::GetActiveAssetDirectory();
	if (EditorUtils::PathIsOrIsUnder(templatePath, assetDirectory))
	{
		error.clear();
		const std::filesystem::path RelativePath = std::filesystem::relative(templatePath, assetDirectory, error).lexically_normal();
		if (!error)
			Project::GetActive()->GetEditorAssetManager()->ImportAsset(RelativePath);
	}

	if (layer.m_ContentBrowserPanel)
		layer.m_ContentBrowserPanel->RefreshAssetTree();

	WHP_EDITOR_INFO(std::string("[Entity Template] Saved ") + templatePath.string());
}

bool EditorEntityTemplateManager::InstantiateEntityTemplate(AssetHandle handle)
{
	EditorLayer& layer = GetLayer();

	if (!layer.HasProjectLoaded() || !layer.m_SceneManager.EditorScene())
		return false;

	Ref<Project> activeProject = Project::GetActive();
	if (!activeProject->GetEditorAssetManager()->IsAssetHandleValid(handle) ||
		activeProject->GetEditorAssetManager()->GetAssetType(handle) != AssetType::Entity)
	{
		return false;
	}

	if (layer.m_SceneManager.State() != EditorSceneState::Edit)
	{
		WHP_EDITOR_WARN("[Entity Template] Templates can only be instantiated while editing.");
		return false;
	}

	const std::filesystem::path templatePath = Project::GetActiveAssetDirectory() / activeProject->GetEditorAssetManager()->GetFilepath(handle);
	layer.m_HistoryManager.CaptureSceneHistory();
	SceneSerializer serializer(layer.m_SceneManager.EditorScene());
	Entity instance = serializer.DeserializeEntityTemplate(templatePath, handle);
	if (!instance)
	{
		WHP_EDITOR_ERROR(std::string("[Entity Template] Could not instantiate template: ") + templatePath.string());
		return false;
	}

	layer.m_SceneHierarchyPanel.SetSelectedEntity(instance);
	WHP_EDITOR_INFO(std::string("[Entity Template] Instantiated ") + templatePath.string());
	return true;
}

Entity EditorEntityTemplateManager::FindPrefabRoot(Entity entityIn) const
{
	if (!entityIn || !entityIn.HasComponent<PrefabComponent>())
		return {};

	const AssetHandle source = entityIn.GetComponent<PrefabComponent>().m_Source;
	Entity current = entityIn;
	while (current && current.HasComponent<HierarchyComponent>())
	{
		if (current.HasComponent<PrefabComponent>())
		{
			const auto& prefab = current.GetComponent<PrefabComponent>();
			if (prefab.m_Source == source && prefab.m_Root)
				return current;
		}

		const auto& hierarchy = current.GetComponent<HierarchyComponent>();
		if (hierarchy.m_Parent == 0)
			break;

		EditorLayer& layer = GetLayer();
		current = layer.m_SceneManager.EditorScene() ? layer.m_SceneManager.EditorScene()->FindEntityByUUID(hierarchy.m_Parent) : Entity{};
	}

	return entityIn.GetComponent<PrefabComponent>().m_Root ? entityIn : Entity{};
}

void EditorEntityTemplateManager::RemovePrefabLinksRecursive(Entity entityIn)
{
	if (!entityIn)
		return;

	std::vector<UUID> children;
	if (entityIn.HasComponent<HierarchyComponent>())
		children = entityIn.GetComponent<HierarchyComponent>().m_Children;

	if (entityIn.HasComponent<PrefabComponent>())
		entityIn.RemoveComponent<PrefabComponent>();

	EditorLayer& layer = GetLayer();
	for (UUID childId : children)
	{
		if (Entity child = layer.m_SceneManager.EditorScene() ? layer.m_SceneManager.EditorScene()->FindEntityByUUID(childId) : Entity{}; child)
			RemovePrefabLinksRecursive(child);
	}
}

void EditorEntityTemplateManager::ApplyEntityTemplate(Entity entityIn)
{
	EditorLayer& layer = GetLayer();

	if (!layer.HasProjectLoaded() || !layer.m_SceneManager.EditorScene())
		return;

	Entity root = FindPrefabRoot(entityIn);
	if (!root || !root.HasComponent<PrefabComponent>())
	{
		WHP_EDITOR_WARN("[Entity Template] Apply failed. Select a template instance root or child.");
		return;
	}

	Ref<Project> activeProject = Project::GetActive();
	AssetHandle handle = root.GetComponent<PrefabComponent>().m_Source;
	if (!activeProject->GetEditorAssetManager()->IsAssetHandleValid(handle) ||
		activeProject->GetEditorAssetManager()->GetAssetType(handle) != AssetType::Entity)
	{
		WHP_EDITOR_WARN("[Entity Template] Apply failed. The source template Asset is missing.");
		return;
	}

	const std::filesystem::path templatePath = Project::GetActiveAssetDirectory() / activeProject->GetEditorAssetManager()->GetFilepath(handle);
	std::error_code error;
	if (!std::filesystem::exists(templatePath, error))
	{
		WHP_EDITOR_WARN(std::string("[Entity Template] Apply failed. File is missing: ") + templatePath.string());
		return;
	}

	SceneSerializer serializer(layer.m_SceneManager.EditorScene());
	if (!serializer.SerializeEntityTemplate(root, templatePath))
	{
		WHP_EDITOR_ERROR(std::string("[Entity Template] Could not apply instance to template: ") + templatePath.string());
		return;
	}

	activeProject->GetEditorAssetManager()->UnloadAsset(handle);
	if (layer.m_ContentBrowserPanel)
		layer.m_ContentBrowserPanel->RefreshAssetTree();

	WHP_EDITOR_INFO(std::string("[Entity Template] Applied instance to ") + templatePath.string());
}

void EditorEntityTemplateManager::RevertEntityTemplate(Entity entityIn)
{
	EditorLayer& layer = GetLayer();
	if (!layer.HasProjectLoaded() || !layer.m_SceneManager.EditorScene())
		return;

	Entity root = FindPrefabRoot(entityIn);
	if (!root || !root.HasComponent<PrefabComponent>())
	{
		WHP_EDITOR_WARN("[Entity Template] Revert failed. Select a template instance root or child.");
		return;
	}

	Ref<Project> activeProject = Project::GetActive();
	AssetHandle handle = root.GetComponent<PrefabComponent>().m_Source;
	if (!activeProject->GetEditorAssetManager()->IsAssetHandleValid(handle) ||
		activeProject->GetEditorAssetManager()->GetAssetType(handle) != AssetType::Entity)
	{
		WHP_EDITOR_WARN("[Entity Template] Revert failed. The source template Asset is missing.");
		return;
	}

	const std::filesystem::path templatePath = Project::GetActiveAssetDirectory() / activeProject->GetEditorAssetManager()->GetFilepath(handle);
	std::error_code error;
	if (!std::filesystem::exists(templatePath, error))
	{
		WHP_EDITOR_WARN(std::string("[Entity Template] Revert failed. File is missing: ") + templatePath.string());
		return;
	}

	{
		Ref<Scene> validationScene = MakeRef<Scene>();
		SceneSerializer validator(validationScene);
		if (!validator.DeserializeEntityTemplate(templatePath, handle))
		{
			WHP_EDITOR_ERROR(std::string("[Entity Template] Revert failed. Could not read template: ") + templatePath.string());
			return;
		}
	}

	UUID parentId = 0;
	size_t childIndex = 0;
	bool hadChildIndex = false;
	if (root.HasComponent<HierarchyComponent>())
	{
		const auto& hierarchy = root.GetComponent<HierarchyComponent>();
		parentId = hierarchy.m_Parent;
		if (parentId != 0)
		{
			Entity parent = layer.m_SceneManager.EditorScene()->FindEntityByUUID(parentId);
			if (parent && parent.HasComponent<HierarchyComponent>())
			{
				const auto& siblings = parent.GetComponent<HierarchyComponent>().m_Children;
				auto siblingIt = std::find(siblings.begin(), siblings.end(), root.GetUUID());
				if (siblingIt != siblings.end())
				{
					childIndex = static_cast<size_t>(std::distance(siblings.begin(), siblingIt));
					hadChildIndex = true;
				}
			}
		}
	}

	TransformComponent preservedTransform{};
	if (root.HasComponent<TransformComponent>())
		preservedTransform = root.GetComponent<TransformComponent>();

	layer.m_HistoryManager.CaptureSceneHistory();
	layer.m_SceneManager.EditorScene()->DestroyEntity(root);

	SceneSerializer serializer(layer.m_SceneManager.EditorScene());
	Entity reverted = serializer.DeserializeEntityTemplate(templatePath, handle);
	if (!reverted)
	{
		WHP_EDITOR_ERROR(std::string("[Entity Template] Revert failed after validation: ") + templatePath.string());
		return;
	}

	if (reverted.HasComponent<TransformComponent>())
		reverted.GetComponent<TransformComponent>() = preservedTransform;

	if (parentId != 0 && reverted.HasComponent<HierarchyComponent>())
	{
		Entity parent = layer.m_SceneManager.EditorScene()->FindEntityByUUID(parentId);
		if (parent && parent.HasComponent<HierarchyComponent>())
		{
			auto& hierarchy = reverted.GetComponent<HierarchyComponent>();
			hierarchy.m_Parent = parentId;

			auto& siblings = parent.GetComponent<HierarchyComponent>().m_Children;
			siblings.erase(std::remove(siblings.begin(), siblings.end(), reverted.GetUUID()), siblings.end());
			size_t insertIndex = hadChildIndex ? std::min(childIndex, siblings.size()) : siblings.size();
			siblings.insert(siblings.begin() + static_cast<std::vector<UUID>::difference_type>(insertIndex), reverted.GetUUID());
		}
	}

	layer.m_SceneHierarchyPanel.SetSelectedEntity(reverted);
	WHP_EDITOR_INFO(std::string("[Entity Template] Reverted instance from ") + templatePath.string());
}

void EditorEntityTemplateManager::UnpackEntityTemplate(Entity entityIn)
{
	EditorLayer& layer = GetLayer();
	if (!layer.HasProjectLoaded() || !layer.m_SceneManager.EditorScene())
		return;

	Entity root = FindPrefabRoot(entityIn);
	if (!root)
	{
		WHP_EDITOR_WARN("[Entity Template] Unpack failed. Select a template instance root or child.");
		return;
	}

	layer.m_HistoryManager.CaptureSceneHistory();
	RemovePrefabLinksRecursive(root);
	layer.m_SceneHierarchyPanel.SetSelectedEntity(root);
	WHP_EDITOR_INFO(std::string("[Entity Template] Unpacked instance ") + root.GetName());
}

_WHIP_END
