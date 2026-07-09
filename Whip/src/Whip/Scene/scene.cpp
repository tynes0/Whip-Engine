#include "WhipPch.h"
#include <Whip/Scene/Scene.h>

#include <Whip/Scene/Components.h>

#include <Whip/Scripting/ScriptEngine.h>
#include <Whip/Render/RenderCommand.h>
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

	UIRect BuildUIRect(const UITransformComponent& transform, const glm::vec2& containerMin, const glm::vec2& containerSize)
	{
		const glm::vec2 anchorMin = containerMin + transform.m_AnchorMin * containerSize;
		const glm::vec2 anchorMax = containerMin + transform.m_AnchorMax * containerSize;
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

	UIRect ResolveUIRect(Scene& scene, entt::registry& registry, entt::entity entity, const glm::vec2& viewportSize, uint32_t depth = 0)
	{
		glm::vec2 containerMin{ 0.0f, 0.0f };
		glm::vec2 containerSize = viewportSize;

		if (depth < 32 && registry.any_of<HierarchyComponent>(entity))
		{
			const auto& hierarchy = registry.get<HierarchyComponent>(entity);
			if (hierarchy.m_Parent != 0)
			{
				Entity parent = scene.FindEntityByUUID(hierarchy.m_Parent);
				if (parent && parent.HasComponent<UITransformComponent>())
				{
					UIRect parentRect = ResolveUIRect(scene, registry, static_cast<entt::entity>(parent), viewportSize, depth + 1);
					containerMin = parentRect.m_Min;
					containerSize = parentRect.m_Size;
				}
			}
		}

		return BuildUIRect(registry.get<UITransformComponent>(entity), containerMin, containerSize);
	}

	bool ContainsPoint(const UIRect& rect, const glm::vec2& point)
	{
		return point.x >= rect.m_Min.x && point.x <= rect.m_Max.x
			&& point.y >= rect.m_Min.y && point.y <= rect.m_Max.y;
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

	float ResolveCrossPosition(UIStackLayoutComponent::Alignment alignment, float contentMin, float contentMax, float childSize)
	{
		switch (alignment)
		{
		case UIStackLayoutComponent::Alignment::Start: return contentMin + childSize * 0.5f;
		case UIStackLayoutComponent::Alignment::End: return contentMax - childSize * 0.5f;
		case UIStackLayoutComponent::Alignment::Center:
		case UIStackLayoutComponent::Alignment::Stretch:
		default: return (contentMin + contentMax) * 0.5f;
		}
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
	m_FocusedUIButton = 0;
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
	m_FocusedUIButton = 0;
}

void Scene::OnSimulationStart()
{
	WHP_PROFILE_FUNCTION();
	m_IsRunning = true;
	m_FocusedUIButton = 0;
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
	m_FocusedUIButton = 0;
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
	{
		Input::SetRuntimeInputCapturedByUI(false);
		return;
	}

	UpdateUILayouts();

	const bool inputActive = Input::IsRuntimeInputActive() && Input::IsMouseInsideViewport();
	glm::vec2 mousePosition = Input::GetMouseViewportPosition();
	mousePosition.y = static_cast<float>(m_ViewportHeight) - mousePosition.y;
	const glm::vec2 viewportSize{ static_cast<float>(m_ViewportWidth), static_cast<float>(m_ViewportHeight) };

	auto buttonView = m_Registry.view<UITransformComponent, UIButtonComponent>();
	for (auto entity : buttonView)
	{
		auto [transform, button] = buttonView.get<UITransformComponent, UIButtonComponent>(entity);
		button.m_ClickedThisFrame = false;
		button.m_SubmittedThisFrame = false;
		button.m_Hovered = false;
		button.m_Pressed = false;
		button.m_Focused = false;
	}

	bool capturedByUI = false;
	entt::entity hoveredButton = entt::null;
	int32_t hoveredSortOrder = 0;
	bool hasTopmostRaycastTarget = false;
	std::vector<entt::entity> navigableButtons;

	auto setTopmostRaycastTarget = [&](entt::entity entity, int32_t sortOrder, bool isInteractableButton)
		{
			capturedByUI = true;
			if (!hasTopmostRaycastTarget || sortOrder >= hoveredSortOrder)
			{
				hasTopmostRaycastTarget = true;
				hoveredSortOrder = sortOrder;
				hoveredButton = isInteractableButton ? entity : entt::null;
			}
		};

	if (inputActive)
	{
		auto imageView = m_Registry.view<UITransformComponent, UIImageComponent>();
		for (auto entity : imageView)
		{
			auto [transform, image] = imageView.get<UITransformComponent, UIImageComponent>(entity);
			if (!transform.m_Visible || !image.m_RaycastTarget)
				continue;

			const UIRect rect = ResolveUIRect(*this, m_Registry, entity, viewportSize);
			if (ContainsPoint(rect, mousePosition))
				setTopmostRaycastTarget(entity, transform.m_SortOrder, false);
		}

		for (auto entity : buttonView)
		{
			auto [transform, button] = buttonView.get<UITransformComponent, UIButtonComponent>(entity);
			if (!transform.m_Visible || !button.m_RaycastTarget)
				continue;

			const UIRect rect = ResolveUIRect(*this, m_Registry, entity, viewportSize);
			if (!ContainsPoint(rect, mousePosition))
				continue;

			setTopmostRaycastTarget(entity, transform.m_SortOrder, button.m_Interactable);
		}

		for (auto entity : buttonView)
		{
			auto [transform, button] = buttonView.get<UITransformComponent, UIButtonComponent>(entity);
			if (transform.m_Visible && button.m_Interactable && button.m_RaycastTarget && button.m_NavigationEnabled)
				navigableButtons.push_back(entity);
		}
	}

	std::sort(navigableButtons.begin(), navigableButtons.end(), [&](entt::entity left, entt::entity right)
		{
			const auto& leftTransform = m_Registry.get<UITransformComponent>(left);
			const auto& rightTransform = m_Registry.get<UITransformComponent>(right);
			if (leftTransform.m_SortOrder != rightTransform.m_SortOrder)
				return leftTransform.m_SortOrder < rightTransform.m_SortOrder;

			const UIRect leftRect = ResolveUIRect(*this, m_Registry, left, viewportSize);
			const UIRect rightRect = ResolveUIRect(*this, m_Registry, right, viewportSize);
			if (leftRect.m_Center.y != rightRect.m_Center.y)
				return leftRect.m_Center.y > rightRect.m_Center.y;
			return leftRect.m_Center.x < rightRect.m_Center.x;
		});

	Entity focusedEntity = m_FocusedUIButton ? FindEntityByUUID(m_FocusedUIButton) : Entity{};
	auto focusedIt = focusedEntity ? std::find(navigableButtons.begin(), navigableButtons.end(), static_cast<entt::entity>(focusedEntity)) : navigableButtons.end();
	if (focusedIt == navigableButtons.end())
	{
		m_FocusedUIButton = 0;
		focusedEntity = {};
	}

	if (hoveredButton != entt::null && Input::IsMouseButtonPressed(Mouse::ButtonLeft))
	{
		auto& hoveredComponent = m_Registry.get<UIButtonComponent>(hoveredButton);
		if (hoveredComponent.m_NavigationEnabled)
		{
			Entity hoveredEntity{ hoveredButton, this };
			m_FocusedUIButton = hoveredEntity.GetUUID();
			focusedEntity = hoveredEntity;
			focusedIt = std::find(navigableButtons.begin(), navigableButtons.end(), hoveredButton);
		}
	}

	const bool shiftHeld = Input::IsKeyDown(Key::LeftShift) || Input::IsKeyDown(Key::RightShift);
	const bool nextRequested = (Input::IsKeyPressed(Key::Tab) && !shiftHeld) || Input::IsKeyPressed(Key::Down) || Input::IsKeyPressed(Key::Right);
	const bool previousRequested = (Input::IsKeyPressed(Key::Tab) && shiftHeld) || Input::IsKeyPressed(Key::Up) || Input::IsKeyPressed(Key::Left);
	const bool submitRequested = Input::IsKeyPressed(Key::Enter) || Input::IsKeyPressed(Key::KPEnter) || Input::IsKeyPressed(Key::Space);
	const bool navigationUsed = inputActive && !navigableButtons.empty() && (nextRequested || previousRequested || submitRequested);
	if (navigationUsed)
	{
		capturedByUI = true;
		if (focusedIt == navigableButtons.end())
			focusedIt = navigableButtons.begin();

		if (nextRequested || previousRequested)
		{
			size_t focusedIndex = static_cast<size_t>(std::distance(navigableButtons.begin(), focusedIt));
			if (nextRequested)
				focusedIndex = (focusedIndex + 1) % navigableButtons.size();
			else
				focusedIndex = focusedIndex == 0 ? navigableButtons.size() - 1 : focusedIndex - 1;
			focusedIt = navigableButtons.begin() + static_cast<std::ptrdiff_t>(focusedIndex);
		}

		focusedEntity = Entity{ *focusedIt, this };
		m_FocusedUIButton = focusedEntity.GetUUID();
	}

	Input::SetRuntimeInputCapturedByUI(capturedByUI);

	if (focusedEntity && focusedEntity.HasComponent<UIButtonComponent>())
		focusedEntity.GetComponent<UIButtonComponent>().m_Focused = true;

	if (hoveredButton != entt::null)
	{
		auto& button = m_Registry.get<UIButtonComponent>(hoveredButton);
		button.m_Hovered = true;
		button.m_Pressed = Input::IsMouseButtonDown(Mouse::ButtonLeft);
		button.m_ClickedThisFrame = Input::IsMouseButtonReleased(Mouse::ButtonLeft);
		if (button.m_ClickedThisFrame)
			ScriptEngine::InvokeEntityMethod(EntityMethodType::OnUIClick, Entity{ hoveredButton, this });
	}

	if (submitRequested && focusedEntity && focusedEntity.HasComponent<UIButtonComponent>() && inputActive)
	{
		auto& button = focusedEntity.GetComponent<UIButtonComponent>();
		if (button.m_Interactable && button.m_NavigationEnabled)
		{
			button.m_Pressed = true;
			button.m_ClickedThisFrame = true;
			button.m_SubmittedThisFrame = true;
			ScriptEngine::InvokeEntityMethod(EntityMethodType::OnUIClick, focusedEntity);
		}
	}
}

void Scene::UpdateUILayouts()
{
	WHP_PROFILE_FUNCTION();
	if (m_ViewportWidth == 0 || m_ViewportHeight == 0)
		return;

	const glm::vec2 viewportSize{ static_cast<float>(m_ViewportWidth), static_cast<float>(m_ViewportHeight) };
	auto view = m_Registry.view<UITransformComponent, UIStackLayoutComponent, HierarchyComponent>();
	for (auto entity : view)
	{
		auto [layoutTransform, layout, hierarchy] = view.get<UITransformComponent, UIStackLayoutComponent, HierarchyComponent>(entity);
		if (!layoutTransform.m_Visible || hierarchy.m_Children.empty())
			continue;

		const UIRect parentRect = ResolveUIRect(*this, m_Registry, entity, viewportSize);
		const float contentMinX = parentRect.m_Min.x + layout.m_Padding.x;
		const float contentMaxX = parentRect.m_Max.x - layout.m_Padding.z;
		const float contentMinY = parentRect.m_Min.y + layout.m_Padding.w;
		const float contentMaxY = parentRect.m_Max.y - layout.m_Padding.y;
		const float contentWidth = std::max(contentMaxX - contentMinX, 1.0f);
		const float contentHeight = std::max(contentMaxY - contentMinY, 1.0f);

		std::vector<Entity> children;
		children.reserve(hierarchy.m_Children.size());
		for (UUID childId : hierarchy.m_Children)
		{
			Entity child = FindEntityByUUID(childId);
			if (child && child.HasComponent<UITransformComponent>())
				children.push_back(child);
		}
		if (children.empty())
			continue;
		if (layout.m_Reverse)
			std::reverse(children.begin(), children.end());

		if (layout.m_Axis == UIStackLayoutComponent::Axis::Horizontal)
		{
			float totalWidth = 0.0f;
			for (Entity child : children)
			{
				auto& childTransform = child.GetComponent<UITransformComponent>();
				if (layout.m_ControlChildWidth)
					childTransform.m_Size.x = layout.m_ChildSize.x;
				if (layout.m_ControlChildHeight || layout.m_Alignment == UIStackLayoutComponent::Alignment::Stretch)
					childTransform.m_Size.y = layout.m_Alignment == UIStackLayoutComponent::Alignment::Stretch ? contentHeight : layout.m_ChildSize.y;
				totalWidth += childTransform.m_Size.x;
			}
			totalWidth += layout.m_Spacing * static_cast<float>(children.size() - 1);

			float currentX = contentMinX;
			if (layout.m_Alignment == UIStackLayoutComponent::Alignment::Center)
				currentX = contentMinX + std::max(contentWidth - totalWidth, 0.0f) * 0.5f;
			else if (layout.m_Alignment == UIStackLayoutComponent::Alignment::End)
				currentX = contentMaxX - totalWidth;

			for (Entity child : children)
			{
				auto& childTransform = child.GetComponent<UITransformComponent>();
				const float childY = ResolveCrossPosition(layout.m_Alignment, contentMinY, contentMaxY, childTransform.m_Size.y);
				const glm::vec2 desiredCenter{ currentX + childTransform.m_Size.x * 0.5f, childY };
				childTransform.m_AnchorMin = { 0.0f, 0.0f };
				childTransform.m_AnchorMax = { 0.0f, 0.0f };
				childTransform.m_Pivot = { 0.5f, 0.5f };
				childTransform.m_AnchoredPosition = desiredCenter - parentRect.m_Min;
				currentX += childTransform.m_Size.x + layout.m_Spacing;
			}
		}
		else
		{
			float totalHeight = 0.0f;
			for (Entity child : children)
			{
				auto& childTransform = child.GetComponent<UITransformComponent>();
				if (layout.m_ControlChildWidth || layout.m_Alignment == UIStackLayoutComponent::Alignment::Stretch)
					childTransform.m_Size.x = layout.m_Alignment == UIStackLayoutComponent::Alignment::Stretch ? contentWidth : layout.m_ChildSize.x;
				if (layout.m_ControlChildHeight)
					childTransform.m_Size.y = layout.m_ChildSize.y;
				totalHeight += childTransform.m_Size.y;
			}
			totalHeight += layout.m_Spacing * static_cast<float>(children.size() - 1);

			float currentY = contentMaxY;
			if (layout.m_Alignment == UIStackLayoutComponent::Alignment::Center)
				currentY = contentMaxY - std::max(contentHeight - totalHeight, 0.0f) * 0.5f;
			else if (layout.m_Alignment == UIStackLayoutComponent::Alignment::End)
				currentY = contentMinY + totalHeight;

			for (Entity child : children)
			{
				auto& childTransform = child.GetComponent<UITransformComponent>();
				const float childX = ResolveCrossPosition(layout.m_Alignment, contentMinX, contentMaxX, childTransform.m_Size.x);
				const glm::vec2 desiredCenter{ childX, currentY - childTransform.m_Size.y * 0.5f };
				childTransform.m_AnchorMin = { 0.0f, 0.0f };
				childTransform.m_AnchorMax = { 0.0f, 0.0f };
				childTransform.m_Pivot = { 0.5f, 0.5f };
				childTransform.m_AnchoredPosition = desiredCenter - parentRect.m_Min;
				currentY -= childTransform.m_Size.y + layout.m_Spacing;
			}
		}
	}
}

void Scene::RenderUIOverlay()
{
	WHP_PROFILE_FUNCTION();
	if (m_ViewportWidth == 0 || m_ViewportHeight == 0)
		return;

	UpdateUILayouts();

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
	RenderCommand::SetDepthTest(false);
	Renderer2D::BeginScene(uiCamera);

	for (size_t index = 0; index < items.size(); ++index)
	{
		const entt::entity entity = items[index].m_Entity;
		const auto& transform = m_Registry.get<UITransformComponent>(entity);
		const UIRect rect = ResolveUIRect(*this, m_Registry, entity, viewportSize);
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
			else if (button.m_Focused)
				color = glm::mix(button.m_NormalColor, button.m_FocusColor, 0.25f);

			Renderer2D::DrawQuad(BuildUITransform(rect, transform, z + 0.00025f), color, entityId);
			if (button.m_Focused && button.m_Interactable)
				Renderer2D::DrawRect(BuildUITransform(rect, transform, z + 0.00045f), button.m_FocusColor, entityId);
			DrawUIText(button.m_Text, button.m_Font, button.m_TextColor, button.m_FontSize, 0.0f, 0.0f, rect, z + 0.0005f, entityId);
		}

		if (m_Registry.any_of<UITextComponent>(entity))
		{
			const auto& text = m_Registry.get<UITextComponent>(entity);
			DrawUIText(text.m_TextString, text.m_Font, text.m_Color, text.m_FontSize, text.m_Kerning, text.m_LineSpacing, rect, z + 0.00075f, entityId);
		}
	}

	Renderer2D::EndScene();
	RenderCommand::SetDepthTest(true);
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
void Scene::OnComponentAdded<UIStackLayoutComponent>(Entity entityIn, UIStackLayoutComponent& component)
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
