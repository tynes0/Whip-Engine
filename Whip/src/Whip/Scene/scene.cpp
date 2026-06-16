#include "WhipPch.h"
#include <Whip/Scene/Scene.h>

#include <Whip/Scene/Components.h>
#include <Whip/Scene/ScriptableEntity.h>

#include <Whip/Core/Input.h>
#include <Whip/Scripting/ScriptEngine.h>
#include <Whip/Render/Renderer2D.h>

#include <Whip/Physics/Physics2D.h>
#include <Whip/Physics/PhysicsWorld.h>

#include <Whip/Project/Project.h>
#include <Whip/Audio/AudioEngine.h>
#include <Whip/Animation/AnimationManager.h>
#include <Whip/Animation/AnimationController.h>
#include <Whip/Asset/AssetManager.h>

#include <glm/glm.hpp>

#include <coco.h>

#include <Whip/Scene/Entity.h>

#include <algorithm>
#include <functional>

#include "box2d/b2_world.h"
#include "box2d/b2_body.h"
#include "box2d/b2_fixture.h"
#include "box2d/b2_polygon_shape.h"
#include "box2d/b2_circle_shape.h"

_WHIP_START

namespace Utils
{
	template<class... Components>
	static void CopyComponent(entt::registry& dst, entt::registry& src, const std::unordered_map<UUID, entt::entity>& enttMap)
	{
		([&]()
			{
				auto view = src.view<Components>();
				for (auto srcEntity : view)
				{
					entt::entity dstEntity = enttMap.at(src.get<IDComponent>(srcEntity).m_ID);

					auto& srcComponent = src.get<Components>(srcEntity);
					dst.emplace_or_replace<Components>(dstEntity, srcComponent);
				}
			}(), ...);
	}

	template<class... Components>
	static void CopyComponent(ComponentGroup<Components...>, entt::registry& dst, entt::registry& src, const std::unordered_map<UUID, entt::entity>& enttMap)
	{
		CopyComponent<Components...>(dst, src, enttMap);
	}

	template<class... Components>
	static void CopyComponentIfExists(Entity dst, Entity src)
	{
		([&]()
			{
				if (src.HasComponent<Components>())
					dst.AddOrReplaceComponent<Components>(src.GetComponent<Components>());
			}(), ...);
	}

	template<class... Components>
	static void CopyComponentIfExists(ComponentGroup<Components...>, Entity dst, Entity src)
	{
		CopyComponentIfExists<Components...>(dst, src);
	}
}

Scene::Scene(AssetHandle handle) : Asset(handle)
{
	m_PhysicsWorld.SetSceneContext(this);
}

Scene::~Scene() {}

Ref<Scene> Scene::Copy(Ref<Scene> other)
{
	WHP_PROFILE_FUNCTION();
	Ref<Scene> newScene = MakeRef<Scene>(other->m_Handle);

	newScene->m_ViewportWidth = other->m_ViewportWidth;
	newScene->m_ViewportHeight = other->m_ViewportHeight;

	auto& srcSceneRegistry = other->m_Registry;
	auto& dstSceneRegistry = newScene->m_Registry;
	std::unordered_map<UUID, entt::entity> enttMap;

	auto idView = srcSceneRegistry.view<IDComponent>();
	for (auto e : idView)
	{
		UUID uuid = srcSceneRegistry.get<IDComponent>(e).m_ID;
		const auto& name = srcSceneRegistry.get<TagComponent>(e).m_Tag;
		Entity newEntity = newScene->CreateEntityWithUUID(uuid, name);
		enttMap[uuid] = (entt::entity)newEntity;
	}

	Utils::CopyComponent(AllComponentsNoIDNoTag{}, dstSceneRegistry, srcSceneRegistry, enttMap);

	return newScene;
}

Entity Scene::CreateEntity(const std::string& name)
{
	return CreateEntityWithUUID(UUID(), name);
}

Entity Scene::CreateEntityWithUUID(UUID uuid, const std::string& name)
{
	Entity result = { m_Registry.create(), this };
	result.AddComponent<IDComponent>(IDComponent(uuid));
	result.AddComponent<TransformComponent>();
	result.AddComponent<HierarchyComponent>();
	auto& tag = result.AddComponent<TagComponent>();
	tag.m_Tag = m_UniqueNameManager.AddName(name);
	m_EntityMap[uuid] = result;
	return result;
}

void Scene::DestroyEntity(Entity entityIn)
{
	if (entityIn.HasComponent<HierarchyComponent>())
	{
		auto hierarchy = entityIn.GetComponent<HierarchyComponent>();
		for (UUID childId : hierarchy.m_Children)
		{
			Entity child = FindEntityByUUID(childId);
			if (child)
				DestroyEntity(child);
		}

		if (hierarchy.m_Parent != 0)
		{
			Entity parent = FindEntityByUUID(hierarchy.m_Parent);
			if (parent && parent.HasComponent<HierarchyComponent>())
			{
				auto& parentHierarchy = parent.GetComponent<HierarchyComponent>();
				std::erase(parentHierarchy.m_Children, entityIn.GetUUID());
			}
		}
	}

	if (m_IsRunning && entityIn.HasComponent<ScriptComponent>())
		ScriptEngine::InvokeEntityMethod(EntityMethodType::OnDestroy, entityIn);

	m_AnimatorRuntimes.erase(entityIn.GetUUID());
	m_UniqueNameManager.RemoveName(entityIn.GetName());
	m_EntityMap.erase(entityIn.GetUUID());
    m_Registry.destroy(entityIn);
}

Entity Scene::FindEntityByUUID(UUID uuid)
{
	auto it = m_EntityMap.find(uuid);
	if (it != m_EntityMap.end())
		return { it->second, this };
	return Entity{};
}

Entity Scene::FindEntityByName(std::string_view name)
{
	auto view = m_Registry.view<TagComponent>();
	for (auto entityID : view)
	{
		const TagComponent& tagComponent = view.get<TagComponent>(entityID);
		if (tagComponent.m_Tag == name)
			return Entity{ entityID, this };
	}
	return Entity{};
}

void Scene::OnRuntimeStart()
{
	WHP_PROFILE_FUNCTION();
	m_IsRunning = true;
	m_PhysicsWorld.Create();
	{
		ScriptEngine::OnRuntimeStart(this);
		auto view = m_Registry.view<ScriptComponent>();
		for (auto e : view)
		{
			Entity ent = { e, this };
			ScriptEngine::InvokeEntityMethod(EntityMethodType::OnCreate, ent);
		}
		// ScriptEngine::InvokeAllOnCreateMethods();
	}
	CreateAnimatorRuntimes();
}

void Scene::OnRuntimeStop()
{
	WHP_PROFILE_FUNCTION();
	if (m_IsRunning)
		ScriptEngine::InvokeAllOnDestroyMethods();

	m_IsRunning = false;
	m_PhysicsWorld.Destroy();
	ScriptEngine::OnRuntimeStop();
	OnAudiosStop();
	ClearAnimatorRuntimes();
}

void Scene::OnSimulationStart()
{
	WHP_PROFILE_FUNCTION();
	m_IsRunning = true;
	m_PhysicsWorld.Create();
	{
		ScriptEngine::OnRuntimeStart(this);
		auto view = m_Registry.view<ScriptComponent>();
		for (auto e : view)
		{
			Entity ent = { e, this };
			ScriptEngine::InvokeEntityMethod(EntityMethodType::OnCreate, ent);
		}
		// ScriptEngine::InvokeAllOnCreateMethods();
	}
	CreateAnimatorRuntimes();
}

void Scene::OnSimulationStop()
{
	WHP_PROFILE_FUNCTION();
	if (m_IsRunning)
		ScriptEngine::InvokeAllOnDestroyMethods();

	m_IsRunning = false;
	m_PhysicsWorld.Destroy();
	ScriptEngine::OnRuntimeStop();
	OnAudiosStop();
	ClearAnimatorRuntimes();
}

void Scene::OnUpdateRuntime(Timestep ts)
{
	WHP_PROFILE_FUNCTION();
	if(!m_IsPaused || m_StepFrames-- > 0)
	{
		{
			{
				// C# OnUpdate
				auto view = m_Registry.view<ScriptComponent>();
				for (auto e : view)
				{
					Entity ent = { e, this };
					float f = ts;
					ScriptEngine::InvokeEntityMethod(EntityMethodType::OnUpdate, ent, Payload::Ref<float>(f));
				}
			}

			m_Registry.view<NativeScriptComponent>().each([=](auto ent, auto& nsc)
				{
					if (!nsc.m_Instance)
					{
						nsc.m_Instance = nsc.m_InstantiateScript();
						nsc.m_Instance->m_Entity = Entity{ ent, this };
						nsc.m_Instance->OnCreate();
					}
					nsc.m_Instance->OnUpdate(ts);
				});
		}

		// Physics
		{
			m_PhysicsWorld.Update(ts);
		}

		// animations
		{
			AnimationManager::Get().Update(ts);
			UpdateAnimators(ts);
		}
	}

    Camera* mainCamera = nullptr;
    glm::mat4 cameraTransform;
    {
        auto group = m_Registry.group<TransformComponent>(entt::get<CameraComponent>);
        for (auto entity : group)
        {
            auto [transform, cam] = group.get<TransformComponent, CameraComponent>(entity);
            if (cam.m_Primary)
            {
                mainCamera = &cam.m_Camera;
                cameraTransform = transform.GetTransform();
                break;
            }
        }
    }
    if (mainCamera)
    {
		// sprites
        Renderer2D::BeginScene(*mainCamera, cameraTransform);
        {
			auto view = m_Registry.view<TransformComponent, SpriteRendererComponent>();
			for (auto ent : view)
			{
				const auto& [transform, sprite] = view.get<TransformComponent, SpriteRendererComponent>(ent);
				Renderer2D::DrawSprite(transform.GetTransform(), sprite, (int)ent);
			}
        }

		// circles
		{
			auto view = m_Registry.view<TransformComponent, CircleRendererComponent>();
			for (auto ent : view)
			{
				const auto& [transform, circle] = view.get<TransformComponent, CircleRendererComponent>(ent);

				Renderer2D::DrawCircle(transform.GetTransform(), circle.m_Color, circle.m_Thickness, circle.m_Fade, (int)ent);
			}
		}


		// texts
		{
			auto view = m_Registry.view<TransformComponent, TextComponent>();
			for (auto entity : view)
			{
				const auto& [transform, text] = view.get<TransformComponent, TextComponent>(entity);

				Renderer2D::DrawString(text.m_TextString, transform.GetTransform(), text, (int)entity);
			}
		}

        Renderer2D::EndScene();
    }
}

void Scene::OnUpdateSimulation(Timestep ts, EditorCamera& cam)
{
	if (!m_IsPaused || m_StepFrames-- > 0)
	{
		{
			// C# OnUpdate
			auto view = m_Registry.view<ScriptComponent>();
			for (auto e : view)
			{
				Entity ent = { e, this };
				float f = ts;
				ScriptEngine::InvokeEntityMethod(EntityMethodType::OnUpdate, ent, Payload::Ref<float>(f));
			}
		}

		m_PhysicsWorld.Update(ts);
		AnimationManager::Get().Update(ts);
		UpdateAnimators(ts);
	}

	RenderScene(cam);
}

void Scene::OnUpdateEditor(Timestep ts, EditorCamera& cam)
{
	WHP_PROFILE_FUNCTION();
	RenderScene(cam);
}

void Scene::OnViewportResize(uint32_t width, uint32_t height)
{
	WHP_PROFILE_FUNCTION();
    m_ViewportWidth = width;
    m_ViewportHeight = height;

    auto group = m_Registry.group<CameraComponent>();
    for (auto entity : group)
    {
        auto& component = group.get<CameraComponent>(entity);
        if (!component.m_FixedAspectRatio)
            component.m_Camera.SetViewportSize(width, height);
    }
}

Entity Scene::DuplicateEntity(Entity entityIn)
{
	WHP_PROFILE_FUNCTION();
	std::string name = entityIn.GetName();
	Entity newEntity = CreateEntity(name);

	Utils::CopyComponentIfExists(AllComponentsNoIDNoTag{}, newEntity, entityIn);
	if (newEntity.HasComponent<HierarchyComponent>())
		newEntity.GetComponent<HierarchyComponent>() = {};
	if (entityIn.HasComponent<ScriptComponent>())
		ScriptEngine::CopyScriptFieldMap(entityIn, newEntity);
	return newEntity;
}

Entity Scene::InstantiateEntityTemplate(Entity sourceEntity, AssetHandle sourceHandle)
{
	WHP_PROFILE_FUNCTION();
	if (!sourceEntity)
		return {};

	Scene* sourceScene = sourceEntity.GetScene();
	std::function<Entity(Entity, bool)> instantiateTree = [&](Entity source, bool root) -> Entity
	{
		Entity newEntity = CreateEntity(source.GetName());
		Utils::CopyComponentIfExists(AllComponentsNoIDNoTag{}, newEntity, source);

		HierarchyComponent sourceHierarchy{};
		if (source.HasComponent<HierarchyComponent>())
			sourceHierarchy = source.GetComponent<HierarchyComponent>();

		if (newEntity.HasComponent<HierarchyComponent>())
		{
			auto& newHierarchy = newEntity.GetComponent<HierarchyComponent>();
			newHierarchy = {};
			newHierarchy.m_IsGroup = sourceHierarchy.m_IsGroup;
		}

		if (sourceHandle != 0)
		{
			auto& prefab = newEntity.HasComponent<PrefabComponent>() ? newEntity.GetComponent<PrefabComponent>() : newEntity.AddComponent<PrefabComponent>();
			prefab.m_Source = sourceHandle;
			prefab.m_SourceEntity = source.GetUUID();
			prefab.m_Root = root;
		}
		else if (newEntity.HasComponent<PrefabComponent>())
		{
			newEntity.RemoveComponent<PrefabComponent>();
		}

		if (source.HasComponent<ScriptComponent>())
			ScriptEngine::CopyScriptFieldMap(source, newEntity);

		if (sourceScene && source.HasComponent<HierarchyComponent>())
		{
			auto& newHierarchy = newEntity.GetComponent<HierarchyComponent>();
			for (UUID childId : sourceHierarchy.m_Children)
			{
				Entity sourceChild = sourceScene->FindEntityByUUID(childId);
				if (!sourceChild)
					continue;

				Entity newChild = instantiateTree(sourceChild, false);
				if (!newChild)
					continue;

				newHierarchy.m_Children.push_back(newChild.GetUUID());
				if (newChild.HasComponent<HierarchyComponent>())
					newChild.GetComponent<HierarchyComponent>().m_Parent = newEntity.GetUUID();
			}
		}

		return newEntity;
	};

	return instantiateTree(sourceEntity, true);
}

Entity Scene::GetPrimaryCameraEntity()
{
	WHP_PROFILE_FUNCTION();
    auto view = m_Registry.view<CameraComponent>();
    for (auto ent : view)
    {
        const auto& camera = view.get<CameraComponent>(ent);
        if (camera.m_Primary)
            return Entity{ ent, this };
    }
    return {};
}

void Scene::Step(int frames)
{
	m_StepFrames = frames;
}

void Scene::OnAudiosStop()
{
	auto view = m_Registry.view<AudioComponent>();
	for (auto e : view)
	{
		const auto& ac = view.get<AudioComponent>(e);
		for (const auto& audioHandle : ac.m_AudioDatas)
		{
			auto asset = Project::GetActive()->GetAssetManager()->GetAsset(audioHandle.m_Audio);
			auto audioAsset = std::static_pointer_cast<AudioSource>(asset);
			AudioEngine::Stop(audioAsset);
		}
	}
}

void Scene::CreateAnimatorRuntimes()
{
	m_AnimatorRuntimes.clear();

	auto view = m_Registry.view<AnimatorComponent>();
	for (auto e : view)
	{
		Entity entity{ e, this };
		auto& component = entity.GetComponent<AnimatorComponent>();
		if (component.m_Controller == 0 || !AssetManager::IsAssetHandleValid(component.m_Controller) || AssetManager::GetAssetType(component.m_Controller) != AssetType::AnimationController)
			continue;

		Ref<AnimationController> controller = AssetManager::GetAsset<AnimationController>(component.m_Controller);
		if (!controller)
			continue;

		AnimatorRuntime& runtime = m_AnimatorRuntimes[entity.GetUUID()];
		runtime.Bind(this, entity.GetUUID(), controller, component.m_InitialState);
		if (component.m_PlayOnStart)
			runtime.Play(component.m_InitialState);
	}
}

void Scene::ClearAnimatorRuntimes()
{
	m_AnimatorRuntimes.clear();
}

void Scene::UpdateAnimators(Timestep ts)
{
	auto view = m_Registry.view<AnimatorComponent>();
	for (auto e : view)
	{
		Entity entity{ e, this };
		auto& component = entity.GetComponent<AnimatorComponent>();
		if (component.m_Controller == 0 || !AssetManager::IsAssetHandleValid(component.m_Controller) || AssetManager::GetAssetType(component.m_Controller) != AssetType::AnimationController)
		{
			m_AnimatorRuntimes.erase(entity.GetUUID());
			continue;
		}

		AnimatorRuntime* runtime = nullptr;
		auto runtimeIt = m_AnimatorRuntimes.find(entity.GetUUID());
		if (runtimeIt != m_AnimatorRuntimes.end())
			runtime = &runtimeIt->second;

		if (!runtime || runtime->GetControllerHandle() != component.m_Controller)
		{
			Ref<AnimationController> controller = AssetManager::GetAsset<AnimationController>(component.m_Controller);
			if (!controller)
				continue;

			AnimatorRuntime& reboundRuntime = m_AnimatorRuntimes[entity.GetUUID()];
			reboundRuntime.Bind(this, entity.GetUUID(), controller, component.m_InitialState);
			if (component.m_PlayOnStart)
				reboundRuntime.Play(component.m_InitialState);
			runtime = &reboundRuntime;
		}

		runtime->Update(ts, component.m_Speed);
	}
}

void Scene::RenderScene(EditorCamera& cam)
{
	WHP_PROFILE_FUNCTION();
	Renderer2D::BeginScene(cam);

	// sprite
	{
		auto view = m_Registry.view<TransformComponent, SpriteRendererComponent>();
		for (auto entity : view)
		{
			const auto& [transform, sprite] = view.get<TransformComponent, SpriteRendererComponent>(entity);

			Renderer2D::DrawSprite(transform.GetTransform(), sprite, (int)entity);
		}
	}

	// circle
	{
		auto view = m_Registry.view<TransformComponent, CircleRendererComponent>();
		for (auto entity : view)
		{
			const auto& [transform, circle] = view.get<TransformComponent, CircleRendererComponent>(entity);

			Renderer2D::DrawCircle(transform.GetTransform(), circle.m_Color, circle.m_Thickness, circle.m_Fade, (int)entity);
		}
	}

	// texts
	{
		auto view = m_Registry.view<TransformComponent, TextComponent>();
		for (auto entity : view)
		{
			const auto& [transform, text] = view.get<TransformComponent, TextComponent>(entity);

			Renderer2D::DrawString(text.m_TextString, transform.GetTransform(), text, (int)entity);
		}
	}

	Renderer2D::EndScene();
}

template<>
void Scene::OnComponentAdded<TransformComponent>(Entity entityIn, TransformComponent& component)
{
}

template<>
void Scene::OnComponentAdded<CameraComponent>(Entity entityIn, CameraComponent& component)
{
	if(m_ViewportWidth > 0 && m_ViewportHeight > 0)
		component.m_Camera.SetViewportSize(m_ViewportWidth, m_ViewportHeight);
}

template<>
void Scene::OnComponentAdded<SpriteRendererComponent>(Entity entityIn, SpriteRendererComponent& component)
{
}

template<>
void Scene::OnComponentAdded<CircleRendererComponent>(Entity entityIn, CircleRendererComponent& component)
{
}

template<>
void Scene::OnComponentAdded<TextComponent>(Entity entityIn, TextComponent& component)
{
}

template<>
void Scene::OnComponentAdded<TagComponent>(Entity entityIn, TagComponent& component)
{
}

template<>
void Scene::OnComponentAdded<HierarchyComponent>(Entity entityIn, HierarchyComponent& component)
{
}

template<>
void Scene::OnComponentAdded<PrefabComponent>(Entity entityIn, PrefabComponent& component)
{
}

template<>
void Scene::OnComponentAdded<ScriptComponent>(Entity entityIn, ScriptComponent& component)
{
}

template<>
void Scene::OnComponentAdded<AnimatorComponent>(Entity entityIn, AnimatorComponent& component)
{
}

template<>
void Scene::OnComponentAdded<NativeScriptComponent>(Entity entityIn, NativeScriptComponent& component)
{
}

template<>
void Scene::OnComponentAdded<Rigidbody2DComponent>(Entity entityIn, Rigidbody2DComponent& component)
{
}

template<>
void Scene::OnComponentAdded<BoxCollider2DComponent>(Entity entityIn, BoxCollider2DComponent& component)
{
}

template<>
void Scene::OnComponentAdded<CircleCollider2DComponent>(Entity entityIn, CircleCollider2DComponent& component)
{
}

template<>
void Scene::OnComponentAdded<IDComponent>(Entity entityIn, IDComponent& component)
{
}

template<>
void Scene::OnComponentAdded<AudioComponent>(Entity entityIn, AudioComponent& component)
{
}

_WHIP_END
