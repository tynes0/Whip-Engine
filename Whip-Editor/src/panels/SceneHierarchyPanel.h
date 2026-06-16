#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Scene/Scene.h>
#include <Whip/Scene/Entity.h>
#include <Whip/Core/Memory.h>

#include <functional>
#include <vector>


_WHIP_START

class SceneHierarchyPanel
{
public:
	SceneHierarchyPanel() = default;
	SceneHierarchyPanel(const Ref<Scene> context);

	void SetContext(const Ref<Scene>& context);
	Ref<Scene>& GetContext() { return m_Context; }

	void OnImGuiRender();
	void SetSceneChangeCallback(std::function<void()> callback) { m_SceneChangeCallback = std::move(callback); }
	void SetSaveEntityTemplateCallback(std::function<void(Entity)> callback) { m_SaveEntityTemplateCallback = std::move(callback); }
	void SetApplyEntityTemplateCallback(std::function<void(Entity)> callback) { m_ApplyEntityTemplateCallback = std::move(callback); }
	void SetRevertEntityTemplateCallback(std::function<void(Entity)> callback) { m_RevertEntityTemplateCallback = std::move(callback); }
	void SetUnpackEntityTemplateCallback(std::function<void(Entity)> callback) { m_UnpackEntityTemplateCallback = std::move(callback); }
	void SetOpen(bool open);
	bool IsOpen() const { return m_Open; }
	bool ConsumeOpenDirty();

	Entity GetSelectedEntity() const { return m_SelectionContext; }
	std::vector<Entity> GetSelectedEntities() const;
	std::vector<UUID> GetSelectedEntityIds() const { return m_SelectionContexts; }
	void SetSelectedEntity(Entity entityIn, bool append = false);
	void SetSelectedEntityIds(const std::vector<UUID>& ids);
	void SelectAll();
	void ClearSelection();
private:
	void DrawEntityNode(Entity entityIn);
	void DrawComponents(Entity entityIn);
	void DrawMultiEditComponents(const std::vector<Entity>& selectedEntities);
	void DrawMultiSharedComponents(const std::vector<Entity>& selectedEntities);
	void DrawMultiCameraComponent(const std::vector<Entity>& selectedEntities);
	void DrawMultiScriptComponent(const std::vector<Entity>& selectedEntities);
	void DrawMultiSpriteRendererComponent(const std::vector<Entity>& selectedEntities);
	void DrawMultiCircleRendererComponent(const std::vector<Entity>& selectedEntities);
	void DrawMultiTextComponent(const std::vector<Entity>& selectedEntities);
	void DrawMultiRigidbody2DComponent(const std::vector<Entity>& selectedEntities);
	void DrawMultiBoxCollider2DComponent(const std::vector<Entity>& selectedEntities);
	void DrawMultiCircleCollider2DComponent(const std::vector<Entity>& selectedEntities);
	void SetEntityParent(Entity child, Entity parent);
	bool CanParentEntity(Entity child, Entity parent) const;
	bool IsDescendantOf(Entity entityIn, UUID ancestorId) const;
	Entity FindPrefabRoot(Entity entityIn) const;
	void DestroyEntityWithSelection(Entity entityIn);
	bool IsSelected(Entity entityIn) const;
	void NotifySceneChange();
	void BeginPropertyEditHistory();
	void TrackPropertyEditHistory();

	template <class T>
	void DisplayAddComponentEntry(const std::string& entryName);
	template <class T>
	size_t CountSelectedWithComponent() const;
	template <class T>
	void AddComponentToSelection();
	template <class T>
	void RemoveComponentFromSelection();
	template <class T>
	void DrawMultiComponentSummary(const char* name, size_t selectedCount);
private:
	Ref<Scene> m_Context;
	Entity m_SelectionContext;
	std::vector<UUID> m_SelectionContexts;
	std::function<void()> m_SceneChangeCallback;
	std::function<void(Entity)> m_SaveEntityTemplateCallback;
	std::function<void(Entity)> m_ApplyEntityTemplateCallback;
	std::function<void(Entity)> m_RevertEntityTemplateCallback;
	std::function<void(Entity)> m_UnpackEntityTemplateCallback;
	bool m_PropertyEditHistoryActive = false;
	bool m_Open = true;
	bool m_OpenDirty = false;
};

_WHIP_END
