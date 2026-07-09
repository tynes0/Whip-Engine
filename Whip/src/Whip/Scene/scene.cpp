#include "WhipPch.h"
#include <Whip/Scene/Scene.h>

#include <Whip/Scene/Components.h>

#include <Whip/Scripting/ScriptEngine.h>
#include <Whip/Render/Renderer2D.h>
#include <Whip/Core/Input.h>

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
#include <vector>

_WHIP_START

namespace
{
	template<class... Components>
	void CopyComponent(entt::registry& dst, entt::registry& src, const std::unordered_map<UUID, entt::entity>& enttMap)
	{
		([&]{
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
	void CopyComponent(ComponentGroup<Components...>, entt::registry& dst, entt::registry& src, const std::unordered_map<UUID, entt::entity>& enttMap)
	{
		CopyComponent<Components...>(dst, src, enttMap);
	}

	template<class... Components>
	void CopyComponentIfExists(Entity dst, Entity src)
	{
		([&]()
			{
				if (src.HasComponent<Components>())
					dst.AddOrReplaceComponent<Components>(src.GetComponent<Components>());
			}(), ...);
	}

	template<class... Components>
	void CopyComponentIfExists(ComponentGroup<Components...>, Entity dst, Entity src)
	{
		CopyComponentIfExists<Components...>(dst, src);
	}

	struct UIRect
	{
		glm::vec2 m_Min{ 0.0f };
		glm::vec2 m_Max{ 0.0f };
		glm::vec2 m_Size{ 0.0f };
		glm::vec2 m_Center{ 0.0f };
	};

	struct UIRenderItem
	{
		entt::entity m_Entity = entt::null;
		int32_t m_SortOrder = 0;
	};

	UIRect BuildUIRect(const UITransformComponent& transform, const glm::vec2& viewportSize)
	{
		const glm::vec2 anchorMin = transform.m_AnchorMin * viewportSize;
		const glm::vec2 anchorMax = transform.m_AnchorMax * viewportSize;
		const glm::vec2 anchorSize = glm::max(anchorMax - anchorMin, glm::vec2(0.0f));
		const glm::vec2 size = glm::max((anchorSize + transform.m_Size) * transform.m_Scale, glm::vec2(1.0f));
		const glm::vec2 pivotPoint = anchorMin + anchorSize * transform.m_Pivot + transform.m_AnchoredPosition;
		const glm::vec2 center = pivotPoint + (glm::vec2(0.5f) - transform.m_Pivot) * size;

		UIRect rect;
		rect.m_Size = size;
		rect.m_Center = center;
		rect.m_Min = center - size * 0.5f;
		rect.m_Max = center + size * 0.5f;
		return rect;
	}

	glm::mat4 BuildUITransform(const UIRect& rect, const UITransformComponent& transform, float z)
	{
		return glm::translate(glm::mat4(1.0f), glm::vec3(rect.m_Center, z))
			* glm::rotate(glm::mat4(1.0f), glm::radians(transform.m_Rotation), glm::vec3(0.0f, 0.0f, 1.0f))
			* glm::scale(glm::mat4(1.0f), glm::vec3(rect.m_Size, 1.0f));
	}

	Ref<Font> ResolveFont(AssetHandle handle)
	{
		if (handle != 0 && AssetManager::IsAssetHandleValid(handle) && AssetManager::GetAssetType(handle) == AssetType::Font)
		{
			if (Ref<Font> font = AssetManager::GetAsset<Font>(handle))
				return font;
		}
		return Font::GetDefault();
	}

	void DrawUIText(const std::string& text, AssetHandle fontHandle, const glm::vec4& color, float fontSize, float kerning, float lineSpacing, const UIRect& rect, float z, int entityId)
	{
		if (text.empty())
			return;

		const float safeFontSize = std::max(fontSize, 1.0f);
		const glm::vec2 textOrigin = rect.m_Min + glm::vec2(8.0f, std::max((rect.m_Size.y - safeFontSize) * 0.5f, 0.0f));
		const glm::mat4 textTransform = glm::translate(glm::mat4(1.0f), glm::vec3(textOrigin, z))
			* glm::scale(glm::mat4(1.0f), glm::vec3(safeFontSize, safeFontSize, 1.0f));
		Renderer2D::DrawString(text, ResolveFont(fontHandle), textTransform, { color, kerning, lineSpacing }, entityId);
	}
}

Scene::Scene(AssetHandle handle) : Asset(handle)
{
	m_PhysicsWorld.SetSceneContext(this);
}

Scene::~Scene() = default;

Ref<Scene> Scene::Copy(const Ref<Scene>& other)
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
		enttMap[uuid] = static_cast<entt::entity>(newEntity);
	}

	CopyComponent(AllComponentsNoIDNoTag{}, dstSceneRegistry, srcSceneRegistry, enttMap);

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
			if (Entity child = FindEntityByUUID(childId); child)
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

AnimatorRuntime* Scene::GetAnimatorRuntime(UUID entityId)
{
	auto it = m_AnimatorRuntimes.find(entityId);
	return it == m_AnimatorRuntimes.end() ? nullptr : &it->second;
}

AnimatorRuntime* Scene::GetOrCreateAnimatorRuntime(Entity entityIn)
{
	if (!entityIn || !entityIn.HasComponent<AnimatorComponent>())
		return nullptr;

	auto& component = entityIn.GetComponent<AnimatorComponent>();
	if (AnimatorRuntime* runtime = GetAnimatorRuntime(entityIn.GetUUID()); runtime && runtime->GetControllerHandle() == component.m_Controller)
		return runtime;

	return CreateAnimatorRuntime(entityIn, component);
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
	if (!m_IsRunning)
		return;

	m_IsRunning = false;
	ScriptEngine::InvokeAllOnDestroyMethods();
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
	if (!m_IsRunning)
		return;

	m_IsRunning = false;
	ScriptEngine::InvokeAllOnDestroyMethods();
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
		UpdateRuntimeUI();

		{
			WHP_PROFILE_SCOPE("Script Update");
			// C# OnUpdate
			auto view = m_Registry.view<ScriptComponent>();
			for (auto e : view)
			{
				Entity ent = { e, this };
				float f = ts;
				ScriptEngine::InvokeEntityMethod(EntityMethodType::OnUpdate, ent, Payload::Ref<float>(f));
			}
		}

		// Physics
		{
			WHP_PROFILE_SCOPE("Physics Update");
			m_PhysicsWorld.Update(ts);
		}

		// animations
		{
			WHP_PROFILE_SCOPE("Animators Update");
			AnimationManager::Get().Update(ts);
			UpdateAnimators(ts);
		}
	}

	{
		WHP_PROFILE_SCOPE("Renderer Update");
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
					Renderer2D::DrawSprite(transform.GetTransform(), sprite, static_cast<int>(ent));
				}
			}

			// circles
			{
				auto view = m_Registry.view<TransformComponent, CircleRendererComponent>();
				for (auto ent : view)
				{
					const auto& [transform, circle] = view.get<TransformComponent, CircleRendererComponent>(ent);

					Renderer2D::DrawCircle(transform.GetTransform(), circle.m_Color, circle.m_Thickness, circle.m_Fade, static_cast<int>(ent));
				}
			}


			// texts
			{
				auto view = m_Registry.view<TransformComponent, TextComponent>();
				for (auto entity : view)
				{
					const auto& [transform, text] = view.get<TransformComponent, TextComponent>(entity);

					Renderer2D::DrawString(text.m_TextString, transform.GetTransform(), text, static_cast<int>(entity));
				}
			}

			Renderer2D::EndScene();
		}
		RenderUIOverlay();
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
		UpdateRuntimeUI();
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

	CopyComponentIfExists(AllComponentsNoIDNoTag{}, newEntity, entityIn);
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
		CopyComponentIfExists(AllComponentsNoIDNoTag{}, newEntity, source);

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
		CreateAnimatorRuntime(entity, entity.GetComponent<AnimatorComponent>());
	}
}

AnimatorRuntime* Scene::CreateAnimatorRuntime(Entity entityIn, AnimatorComponent& component)
{
	if (!entityIn || component.m_Controller == 0 || !AssetManager::IsAssetHandleValid(component.m_Controller) || AssetManager::GetAssetType(component.m_Controller) != AssetType::AnimationController)
		return nullptr;

	Ref<AnimationController> controller = AssetManager::GetAsset<AnimationController>(component.m_Controller);
	if (!controller)
		return nullptr;

	AnimatorRuntime& runtime = m_AnimatorRuntimes[entityIn.GetUUID()];
	runtime.Bind(this, entityIn.GetUUID(), controller, component.m_InitialState);
	if (component.m_PlayOnStart)
		runtime.Play(component.m_InitialState);
	return &runtime;
}

void Scene::ClearAnimatorRuntimes()
{
	m_AnimatorRuntimes.clear();
}

void Scene::UpdateAnimators(Timestep ts)
{
	WHP_PROFILE_FUNCTION();
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
			runtime = CreateAnimatorRuntime(entity, component);
			if (!runtime)
				continue;
		}

		runtime->Update(ts, component.m_Speed);
		for (const std::string& eventName : runtime->GetFiredEvents())
		{
			std::string_view eventView(eventName);
			ScriptEngine::InvokeEntityMethod(EntityMethodType::OnAnimationEvent, entity, Payload::Ref(eventView));
		}
		runtime->ClearFiredEvents();
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

			Renderer2D::DrawSprite(transform.GetTransform(), sprite, static_cast<int>(entity));
		}
	}

	// circle
	{
		auto view = m_Registry.view<TransformComponent, CircleRendererComponent>();
		for (auto entity : view)
		{
			const auto& [transform, circle] = view.get<TransformComponent, CircleRendererComponent>(entity);

			Renderer2D::DrawCircle(transform.GetTransform(), circle.m_Color, circle.m_Thickness, circle.m_Fade, static_cast<int>(entity));
		}
	}

	// texts
	{
		auto view = m_Registry.view<TransformComponent, TextComponent>();
		for (auto entity : view)
		{
			const auto& [transform, text] = view.get<TransformComponent, TextComponent>(entity);

			Renderer2D::DrawString(text.m_TextString, transform.GetTransform(), text, static_cast<int>(entity));
		}
	}

	Renderer2D::EndScene();
	RenderUIOverlay();
}

void Scene::UpdateRuntimeUI()
{
	WHP_PROFILE_FUNCTION();
	if (m_ViewportWidth == 0 || m_ViewportHeight == 0)
		return;

	const bool inputActive = Input::IsRuntimeInputActive() && Input::IsMouseInsideViewport();
	glm::vec2 mousePosition = Input::GetMouseViewportPosition();
	mousePosition.y = static_cast<float>(m_ViewportHeight) - mousePosition.y;
	const glm::vec2 viewportSize{ static_cast<float>(m_ViewportWidth), static_cast<float>(m_ViewportHeight) };

	auto view = m_Registry.view<UITransformComponent, UIButtonComponent>();
	for (auto entity : view)
	{
		auto [transform, button] = view.get<UITransformComponent, UIButtonComponent>(entity);
		button.m_ClickedThisFrame = false;

		const bool canInteract = inputActive && transform.m_Visible && button.m_Interactable && button.m_RaycastTarget;
		if (!canInteract)
		{
			button.m_Hovered = false;
			button.m_Pressed = false;
			continue;
		}

		const UIRect rect = BuildUIRect(transform, viewportSize);
		const bool hovered = mousePosition.x >= rect.m_Min.x && mousePosition.x <= rect.m_Max.x
			&& mousePosition.y >= rect.m_Min.y && mousePosition.y <= rect.m_Max.y;
		button.m_Hovered = hovered;
		button.m_Pressed = hovered && Input::IsMouseButtonDown(Mouse::ButtonLeft);
		button.m_ClickedThisFrame = hovered && Input::IsMouseButtonReleased(Mouse::ButtonLeft);
	}
}

void Scene::RenderUIOverlay()
{
	WHP_PROFILE_FUNCTION();
	if (m_ViewportWidth == 0 || m_ViewportHeight == 0)
		return;

	std::vector<UIRenderItem> items;
	{
		auto view = m_Registry.view<UITransformComponent>();
		for (auto entity : view)
		{
			const auto& transform = view.get<UITransformComponent>(entity);
			if (transform.m_Visible)
				items.push_back({ entity, transform.m_SortOrder });
		}
	}

	if (items.empty())
		return;

	std::sort(items.begin(), items.end(), [](const UIRenderItem& left, const UIRenderItem& right)
		{
			return left.m_SortOrder < right.m_SortOrder;
		});

	const glm::vec2 viewportSize{ static_cast<float>(m_ViewportWidth), static_cast<float>(m_ViewportHeight) };
	OrthographicCamera uiCamera(0.0f, viewportSize.x, 0.0f, viewportSize.y);
	Renderer2D::BeginScene(uiCamera);

	for (size_t index = 0; index < items.size(); ++index)
	{
		const entt::entity entity = items[index].m_Entity;
		const auto& transform = m_Registry.get<UITransformComponent>(entity);
		const UIRect rect = BuildUIRect(transform, viewportSize);
		const float z = static_cast<float>(index) * 0.001f;
		const int entityId = static_cast<int>(entity);

		if (m_Registry.any_of<UIImageComponent>(entity))
		{
			const auto& image = m_Registry.get<UIImageComponent>(entity);
			SpriteRendererComponent sprite;
			sprite.m_Color = image.m_Color;
			sprite.m_Texture = image.m_Texture;
			sprite.m_TextureSpriteIndex = image.m_TextureSpriteIndex;
			Renderer2D::DrawSprite(BuildUITransform(rect, transform, z), sprite, entityId);
		}

		if (m_Registry.any_of<UIButtonComponent>(entity))
		{
			const auto& button = m_Registry.get<UIButtonComponent>(entity);
			glm::vec4 color = button.m_NormalColor;
			if (!button.m_Interactable)
				color = button.m_DisabledColor;
			else if (button.m_Pressed)
				color = button.m_PressedColor;
			else if (button.m_Hovered)
				color = button.m_HoveredColor;

			Renderer2D::DrawQuad(BuildUITransform(rect, transform, z + 0.00025f), color, entityId);
			DrawUIText(button.m_Text, button.m_Font, button.m_TextColor, button.m_FontSize, 0.0f, 0.0f, rect, z + 0.0005f, entityId);
		}

		if (m_Registry.any_of<UITextComponent>(entity))
		{
			const auto& text = m_Registry.get<UITextComponent>(entity);
			DrawUIText(text.m_TextString, text.m_Font, text.m_Color, text.m_FontSize, text.m_Kerning, text.m_LineSpacing, rect, z + 0.00075f, entityId);
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
void Scene::OnComponentAdded<UITransformComponent>(Entity entityIn, UITransformComponent& component)
{
}

template<>
void Scene::OnComponentAdded<UIImageComponent>(Entity entityIn, UIImageComponent& component)
{
}

template<>
void Scene::OnComponentAdded<UITextComponent>(Entity entityIn, UITextComponent& component)
{
}

template<>
void Scene::OnComponentAdded<UIButtonComponent>(Entity entityIn, UIButtonComponent& component)
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
