#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/Memory.h>
#include <Whip/Core/Timestep.h>
#include <Whip/Core/UUID.h>
#include <Whip/Helper/UniqueNameManager.h>
#include <Whip/Asset/Asset.h>
#include <Whip/Animation/AnimatorRuntime.h>
#include <Whip/Render/EditorCamera.h>

#include <Whip/Physics/PhysicsWorld.h>

#include <unordered_map>

#include <entt.hpp>

_WHIP_START

class Entity;
class SceneHierarchyPanel;
struct AnimatorComponent;

class Scene : public Asset
{
public:
	Scene(AssetHandle handle = AssetHandle{});
	~Scene();

	static Ref<Scene> Copy(const Ref<Scene>& other);

	virtual AssetType GetType() const override { return AssetType::Scene; }

	void OnSimulationStart();
	void OnSimulationStop();

	void OnUpdateRuntime(Timestep ts);
	void OnUpdateSimulation(Timestep ts, EditorCamera& cam);
	void OnUpdateEditor(Timestep ts, EditorCamera& cam);
	void OnViewportResize(uint32_t width, uint32_t height);

	Entity DuplicateEntity(Entity entityIn);
	Entity InstantiateEntityTemplate(Entity sourceEntity, AssetHandle sourceHandle = 0);
	Entity GetPrimaryCameraEntity();

	bool IsRunning() const { return m_IsRunning; }
	bool IsPaused() const { return m_IsPaused; }

	void SetPaused(bool paused) { m_IsPaused = paused; }

	void Step(int frames = 1);

	template <class... Components>
	auto GetAllEntitiesWith()
	{
		return m_Registry.view<Components...>();
	}

	Entity CreateEntity(const std::string& name = std::string());
	Entity CreateEntityWithUUID(UUID uuid, const std::string& name = std::string());
	void DestroyEntity(Entity entityIn);
	Entity FindEntityByUUID(UUID id);
	Entity FindEntityByName(std::string_view name);
	AnimatorRuntime* GetAnimatorRuntime(UUID entityId);
	AnimatorRuntime* GetOrCreateAnimatorRuntime(Entity entityIn);

	void OnRuntimeStart();
	void OnRuntimeStop();
private:

	void OnAudiosStop();
	void CreateAnimatorRuntimes();
	AnimatorRuntime* CreateAnimatorRuntime(Entity entityIn, AnimatorComponent& component);
	void ClearAnimatorRuntimes();
	void UpdateAnimators(Timestep ts);

	void RenderScene(EditorCamera& cam);
	void UpdateUILayouts();
	void UpdateRuntimeUI();
	void RenderUIOverlay();

	template<class T>
	void OnComponentAdded(Entity entityIn, T& component);
private:
	friend class Entity;
	friend class SceneSerializer;
	friend class SceneHierarchyPanel;

	entt::registry m_Registry;
	uint32_t m_ViewportWidth = 0;
	uint32_t m_ViewportHeight = 0;
	std::unordered_map<UUID, entt::entity> m_EntityMap;
	UniqueNameManager m_UniqueNameManager;

	bool m_IsRunning = false;
	bool m_IsPaused = false;
	int m_StepFrames = 0;

	PhysicsWorld m_PhysicsWorld;
	std::unordered_map<UUID, AnimatorRuntime> m_AnimatorRuntimes;
	UUID m_FocusedUIButton = 0;
};

_WHIP_END
