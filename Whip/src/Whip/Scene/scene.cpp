#include "WhipPch.h"
#include <Whip/Scene/Scene.h>

#include <Whip/Scene/Components.h>

#include <Whip/Scripting/ScriptEngine.h>
#include <Whip/Render/RenderCommand.h>
#include <Whip/Render/Renderer2D.h>
#include <Whip/Render/MsdfData.h>
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

	enum class UIControlKind : uint8_t
	{
		None = 0,
		Button,
		Toggle,
		Slider,
		InputField
	};

	struct UIInteractionTarget
	{
		entt::entity m_Entity = entt::null;
		UIControlKind m_Kind = UIControlKind::None;
		int32_t m_SortOrder = 0;
	};

	struct UIPointerState
	{
		glm::vec2 m_Position{ 0.0f };
		bool m_Active = false;
		bool m_Down = false;
		bool m_Pressed = false;
		bool m_Released = false;
	};

	UIPointerState ResolvePrimaryPointerState(uint32_t viewportWidth, uint32_t viewportHeight)
	{
		UIPointerState state;
		state.m_Active = Input::IsRuntimeInputActive() && Input::IsMouseInsideViewport();
		if (!state.m_Active)
			return state;

		state.m_Position = Input::GetMouseViewportPosition();
		state.m_Position.y = static_cast<float>(viewportHeight) - state.m_Position.y;
		state.m_Down = Input::IsMouseButtonDown(Mouse::ButtonLeft);
		state.m_Pressed = Input::IsMouseButtonPressed(Mouse::ButtonLeft);
		state.m_Released = Input::IsMouseButtonReleased(Mouse::ButtonLeft);
		return state;
	}

	float NormalizeSliderValue(const UISliderComponent& slider)
	{
		const float range = slider.m_MaxValue - slider.m_MinValue;
		if (std::abs(range) <= 0.0001f)
			return 0.0f;
		return glm::clamp((slider.m_Value - slider.m_MinValue) / range, 0.0f, 1.0f);
	}

	float ValueFromSliderPoint(const UISliderComponent& slider, const UIRect& rect, const glm::vec2& point)
	{
		const float normalized = glm::clamp((point.x - rect.m_Min.x) / std::max(rect.m_Size.x, 1.0f), 0.0f, 1.0f);
		return glm::mix(slider.m_MinValue, slider.m_MaxValue, normalized);
	}

	bool AppendRuntimeTextInput(std::string& text, int32_t maxCharacters)
	{
		bool changed = false;
		const bool shiftHeld = Input::IsKeyDown(Key::LeftShift) || Input::IsKeyDown(Key::RightShift);
		auto appendCharacter = [&](char character)
			{
				if (maxCharacters <= 0 || text.size() < static_cast<size_t>(maxCharacters))
				{
					text.push_back(character);
					changed = true;
				}
			};

		for (int key = Key::A; key <= Key::Z; ++key)
		{
			if (Input::IsKeyPressed(key))
			{
				const char character = static_cast<char>((shiftHeld ? 'A' : 'a') + (key - Key::A));
				appendCharacter(character);
			}
		}

		for (int key = Key::D0; key <= Key::D9; ++key)
			if (Input::IsKeyPressed(key))
				appendCharacter(static_cast<char>('0' + (key - Key::D0)));

		for (int key = Key::KP0; key <= Key::KP9; ++key)
			if (Input::IsKeyPressed(key))
				appendCharacter(static_cast<char>('0' + (key - Key::KP0)));

		if (Input::IsKeyPressed(Key::Space))
			appendCharacter(' ');
		if (Input::IsKeyPressed(Key::Minus))
			appendCharacter(shiftHeld ? '_' : '-');
		if (Input::IsKeyPressed(Key::Period))
			appendCharacter('.');
		if (Input::IsKeyPressed(Key::Comma))
			appendCharacter(',');
		if (Input::IsKeyPressed(Key::Backspace) && !text.empty())
		{
			text.pop_back();
			changed = true;
		}
		return changed;
	}

	void InvokeUIRuntimeCallback(Entity entity, EntityMethodType fallbackMethod, const std::string& callbackName, const Payload& payload = Payload::Null())
	{
		if (!entity)
			return;

		if (!callbackName.empty() && ScriptEngine::InvokeEntityMethodByName(entity, callbackName, payload))
			return;

		ScriptEngine::InvokeEntityMethod(fallbackMethod, entity, payload);
	}

	float ResolveCanvasScale(const UICanvasComponent& canvas, const glm::vec2& viewportSize)
	{
		const glm::vec2 referenceResolution = glm::max(canvas.m_ReferenceResolution, glm::vec2(1.0f));
		float scale = canvas.m_ScaleFactor;
		if (canvas.m_ScaleMode == UICanvasComponent::ScaleMode::ScaleWithScreenSize)
		{
			const float widthScale = viewportSize.x / referenceResolution.x;
			const float heightScale = viewportSize.y / referenceResolution.y;
			const float match = glm::clamp(canvas.m_MatchWidthOrHeight, 0.0f, 1.0f);
			scale *= glm::mix(widthScale, heightScale, match);
		}
		return std::max(scale, 0.001f);
	}

	float ResolveUIScale(Scene& scene, entt::registry& registry, entt::entity entity, const glm::vec2& viewportSize, uint32_t depth = 0)
	{
		if (registry.any_of<UICanvasComponent>(entity))
			return ResolveCanvasScale(registry.get<UICanvasComponent>(entity), viewportSize);

		if (depth >= 32 || !registry.any_of<HierarchyComponent>(entity))
			return 1.0f;

		const auto& hierarchy = registry.get<HierarchyComponent>(entity);
		if (hierarchy.m_Parent == 0)
			return 1.0f;

		Entity parent = scene.FindEntityByUUID(hierarchy.m_Parent);
		if (!parent)
			return 1.0f;

		return ResolveUIScale(scene, registry, static_cast<entt::entity>(parent), viewportSize, depth + 1);
	}

	bool IsUIBranchVisible(Scene& scene, entt::registry& registry, entt::entity entity, uint32_t depth = 0)
	{
		if (registry.any_of<UITransformComponent>(entity) && !registry.get<UITransformComponent>(entity).m_Visible)
			return false;
		if (registry.any_of<UICanvasComponent>(entity) && !registry.get<UICanvasComponent>(entity).m_Visible)
			return false;
		if (depth >= 32 || !registry.any_of<HierarchyComponent>(entity))
			return true;

		const auto& hierarchy = registry.get<HierarchyComponent>(entity);
		if (hierarchy.m_Parent == 0)
			return true;

		Entity parent = scene.FindEntityByUUID(hierarchy.m_Parent);
		if (!parent)
			return true;

		return IsUIBranchVisible(scene, registry, static_cast<entt::entity>(parent), depth + 1);
	}

	UIRect BuildUIRect(const UITransformComponent& transform, const glm::vec2& containerMin, const glm::vec2& containerSize, float uiScale)
	{
		const glm::vec2 anchorMin = containerMin + transform.m_AnchorMin * containerSize;
		const glm::vec2 anchorMax = containerMin + transform.m_AnchorMax * containerSize;
		const glm::vec2 anchorSize = glm::max(anchorMax - anchorMin, glm::vec2(0.0f));
		const glm::vec2 size = glm::max((anchorSize + transform.m_Size * uiScale) * transform.m_Scale, glm::vec2(1.0f));
		const glm::vec2 pivotPoint = anchorMin + anchorSize * transform.m_Pivot + transform.m_AnchoredPosition * uiScale;
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

		const float uiScale = ResolveUIScale(scene, registry, entity, viewportSize);
		return BuildUIRect(registry.get<UITransformComponent>(entity), containerMin, containerSize, uiScale);
	}

	void ResolveUIContainer(Scene& scene, entt::registry& registry, entt::entity entity, const glm::vec2& viewportSize, glm::vec2& outMin, glm::vec2& outSize, uint32_t depth = 0)
	{
		outMin = { 0.0f, 0.0f };
		outSize = viewportSize;
		if (depth >= 32 || !registry.any_of<HierarchyComponent>(entity))
			return;

		const auto& hierarchy = registry.get<HierarchyComponent>(entity);
		if (hierarchy.m_Parent == 0)
			return;

		Entity parent = scene.FindEntityByUUID(hierarchy.m_Parent);
		if (!parent || !parent.HasComponent<UITransformComponent>())
			return;

		const UIRect parentRect = ResolveUIRect(scene, registry, static_cast<entt::entity>(parent), viewportSize, depth + 1);
		outMin = parentRect.m_Min;
		outSize = parentRect.m_Size;
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

	glm::vec2 MeasureUIText(const std::string& text, const Ref<Font>& font, float fontSize, float kerning, float lineSpacing)
	{
		if (text.empty() || !font)
			return { 0.0f, 0.0f };

		const auto& fontGeometry = font->GetMsdfData()->m_FontGeometry;
		const auto& metrics = fontGeometry.getMetrics();
		const double fsScale = 1.0 / (metrics.ascenderY - metrics.descenderY);
		const auto spaceGlyph = fontGeometry.getGlyph(' ');
		const float spaceGlyphAdvance = spaceGlyph ? static_cast<float>(spaceGlyph->getAdvance()) : 1.0f;
		const float lineHeight = static_cast<float>(fsScale * metrics.lineHeight + lineSpacing) * fontSize;

		float maxWidth = 0.0f;
		float currentWidth = 0.0f;
		uint32_t lineCount = 1;
		for (size_t i = 0; i < text.size(); ++i)
		{
			const char character = text[i];
			if (character == '\r')
				continue;
			if (character == '\n')
			{
				maxWidth = std::max(maxWidth, currentWidth);
				currentWidth = 0.0f;
				++lineCount;
				continue;
			}
			if (character == '\t')
			{
				currentWidth += 4.0f * (static_cast<float>(fsScale) * spaceGlyphAdvance + kerning) * fontSize;
				continue;
			}

			double advance = spaceGlyphAdvance;
			if (const auto glyph = fontGeometry.getGlyph(character))
				advance = glyph->getAdvance();
			if (i < text.size() - 1)
				fontGeometry.getAdvance(advance, character, text[i + 1]);
			currentWidth += (static_cast<float>(fsScale * advance) + kerning) * fontSize;
		}

		maxWidth = std::max(maxWidth, currentWidth);
		return { maxWidth, std::max(lineHeight * static_cast<float>(lineCount), fontSize) };
	}

	void DrawUIText(const std::string& text, AssetHandle fontHandle, const glm::vec4& color, float fontSize, float kerning, float lineSpacing, UITextHorizontalAlignment horizontalAlignment, UITextVerticalAlignment verticalAlignment, const UIRect& rect, float z, int entityId)
	{
		if (text.empty())
			return;

		const float safeFontSize = std::max(fontSize, 1.0f);
		const Ref<Font> font = ResolveFont(fontHandle);
		const glm::vec2 textSize = MeasureUIText(text, font, safeFontSize, kerning, lineSpacing);
		constexpr float padding = 8.0f;
		float originX = rect.m_Min.x + padding;
		if (horizontalAlignment == UITextHorizontalAlignment::Center)
			originX = rect.m_Min.x + std::max((rect.m_Size.x - textSize.x) * 0.5f, padding);
		else if (horizontalAlignment == UITextHorizontalAlignment::Right)
			originX = rect.m_Max.x - textSize.x - padding;

		float originY = rect.m_Min.y + std::max((rect.m_Size.y - safeFontSize) * 0.5f, 0.0f);
		if (verticalAlignment == UITextVerticalAlignment::Top)
			originY = rect.m_Max.y - safeFontSize - padding;
		else if (verticalAlignment == UITextVerticalAlignment::Center)
			originY = rect.m_Min.y + std::max((rect.m_Size.y - textSize.y) * 0.5f, 0.0f);
		else if (verticalAlignment == UITextVerticalAlignment::Bottom)
			originY = rect.m_Min.y + padding;

		const glm::vec2 textOrigin{ originX, originY };
		const glm::mat4 textTransform = glm::translate(glm::mat4(1.0f), glm::vec3(textOrigin, z))
			* glm::scale(glm::mat4(1.0f), glm::vec3(safeFontSize, safeFontSize, 1.0f));
		Renderer2D::DrawString(text, font, textTransform, { color, kerning, lineSpacing }, entityId);
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

bool Scene::TryResolveUIRect(Entity entityIn, glm::vec2& center, glm::vec2& size)
{
	if (!entityIn || entityIn.GetScene() != this || !entityIn.HasComponent<UITransformComponent>() || m_ViewportWidth == 0 || m_ViewportHeight == 0)
		return false;

	const glm::vec2 viewportSize{ static_cast<float>(m_ViewportWidth), static_cast<float>(m_ViewportHeight) };
	const UIRect rect = ResolveUIRect(*this, m_Registry, static_cast<entt::entity>(entityIn), viewportSize);
	center = rect.m_Center;
	size = rect.m_Size;
	return true;
}

bool Scene::ApplyUIRectTransform(Entity entityIn, const glm::vec2& center, const glm::vec2& size, float rotationDegrees)
{
	if (!entityIn || entityIn.GetScene() != this || !entityIn.HasComponent<UITransformComponent>() || m_ViewportWidth == 0 || m_ViewportHeight == 0)
		return false;

	auto& transform = entityIn.GetComponent<UITransformComponent>();
	const glm::vec2 viewportSize{ static_cast<float>(m_ViewportWidth), static_cast<float>(m_ViewportHeight) };
	glm::vec2 containerMin{ 0.0f };
	glm::vec2 containerSize = viewportSize;
	ResolveUIContainer(*this, m_Registry, static_cast<entt::entity>(entityIn), viewportSize, containerMin, containerSize);

	const glm::vec2 anchorMin = containerMin + transform.m_AnchorMin * containerSize;
	const glm::vec2 anchorMax = containerMin + transform.m_AnchorMax * containerSize;
	const glm::vec2 anchorSize = glm::max(anchorMax - anchorMin, glm::vec2(0.0f));
	const float uiScale = ResolveUIScale(*this, m_Registry, static_cast<entt::entity>(entityIn), viewportSize);
	const glm::vec2 safeScale = glm::max(glm::abs(transform.m_Scale), glm::vec2(0.001f));
	const glm::vec2 safeSize = glm::max(glm::abs(size), glm::vec2(1.0f));
	const glm::vec2 pivotPoint = center - (glm::vec2(0.5f) - transform.m_Pivot) * safeSize;

	transform.m_AnchoredPosition = (pivotPoint - (anchorMin + anchorSize * transform.m_Pivot)) / uiScale;
	transform.m_Size = glm::max((safeSize / safeScale - anchorSize) / uiScale, glm::vec2(1.0f));
	transform.m_Rotation = rotationDegrees;
	return true;
}

void Scene::OnRuntimeStart()
{
	WHP_PROFILE_FUNCTION();
	m_IsRunning = true;
	m_FocusedUIControl = 0;
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
	m_FocusedUIControl = 0;
}

void Scene::OnSimulationStart()
{
	WHP_PROFILE_FUNCTION();
	m_IsRunning = true;
	m_FocusedUIControl = 0;
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
	m_FocusedUIControl = 0;
}

void Scene::OnUpdateRuntime(Timestep ts)
{
	WHP_PROFILE_FUNCTION();
	OnUpdateRuntimeSystems(ts);
	RenderRuntimeScene();
}

void Scene::OnUpdateRuntimeSystems(Timestep ts)
{
	WHP_PROFILE_FUNCTION();
	if (m_IsPaused && m_StepFrames-- <= 0)
		return;

	UpdateRuntimeUI();

	{
		WHP_PROFILE_SCOPE("Script Update");
		auto view = m_Registry.view<ScriptComponent>();
		for (auto e : view)
		{
			Entity ent = { e, this };
			float f = ts;
			ScriptEngine::InvokeEntityMethod(EntityMethodType::OnUpdate, ent, Payload::Ref<float>(f));
		}
	}

	{
		WHP_PROFILE_SCOPE("Physics Update");
		m_PhysicsWorld.Update(ts);
	}

	{
		WHP_PROFILE_SCOPE("Animators Update");
		AnimationManager::Get().Update(ts);
		UpdateAnimators(ts);
	}
}

void Scene::OnUpdateSimulation(Timestep ts, EditorCamera& cam)
{
	OnUpdateSimulationSystems(ts);
	RenderScene(cam);
}

void Scene::OnUpdateSimulationSystems(Timestep ts)
{
	WHP_PROFILE_FUNCTION();
	if (m_IsPaused && m_StepFrames-- <= 0)
		return;

	{
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

void Scene::RenderScene(EditorCamera& cam, bool renderUIOverlay)
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
	if (renderUIOverlay)
		RenderUIOverlay();
}

void Scene::RenderRuntimeScene()
{
	WHP_PROFILE_FUNCTION();
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
		Renderer2D::BeginScene(*mainCamera, cameraTransform);
		{
			auto view = m_Registry.view<TransformComponent, SpriteRendererComponent>();
			for (auto entity : view)
			{
				const auto& [transform, sprite] = view.get<TransformComponent, SpriteRendererComponent>(entity);
				Renderer2D::DrawSprite(transform.GetTransform(), sprite, static_cast<int>(entity));
			}
		}

		{
			auto view = m_Registry.view<TransformComponent, CircleRendererComponent>();
			for (auto entity : view)
			{
				const auto& [transform, circle] = view.get<TransformComponent, CircleRendererComponent>(entity);
				Renderer2D::DrawCircle(transform.GetTransform(), circle.m_Color, circle.m_Thickness, circle.m_Fade, static_cast<int>(entity));
			}
		}

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

void Scene::UpdateRuntimeUI()
{
	WHP_PROFILE_FUNCTION();
	if (m_ViewportWidth == 0 || m_ViewportHeight == 0)
	{
		Input::SetRuntimeInputCapturedByUI(false);
		return;
	}

	UpdateUILayouts();

	const UIPointerState pointer = ResolvePrimaryPointerState(m_ViewportWidth, m_ViewportHeight);
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

	auto toggleView = m_Registry.view<UITransformComponent, UIToggleComponent>();
	for (auto entity : toggleView)
	{
		auto [transform, toggle] = toggleView.get<UITransformComponent, UIToggleComponent>(entity);
		toggle.m_Hovered = false;
		toggle.m_Pressed = false;
		toggle.m_Focused = false;
		toggle.m_ChangedThisFrame = false;
	}

	auto sliderView = m_Registry.view<UITransformComponent, UISliderComponent>();
	for (auto entity : sliderView)
	{
		auto [transform, slider] = sliderView.get<UITransformComponent, UISliderComponent>(entity);
		slider.m_Hovered = false;
		slider.m_Pressed = false;
		slider.m_Focused = false;
		slider.m_ChangedThisFrame = false;
		slider.m_Value = glm::clamp(slider.m_Value, std::min(slider.m_MinValue, slider.m_MaxValue), std::max(slider.m_MinValue, slider.m_MaxValue));
	}

	auto inputFieldView = m_Registry.view<UITransformComponent, UIInputFieldComponent>();
	for (auto entity : inputFieldView)
	{
		auto [transform, inputField] = inputFieldView.get<UITransformComponent, UIInputFieldComponent>(entity);
		inputField.m_Hovered = false;
		inputField.m_SubmittedThisFrame = false;
		inputField.m_ChangedThisFrame = false;
	}

	bool capturedByUI = false;
	UIInteractionTarget hoveredTarget;
	bool hasTopmostRaycastTarget = false;
	std::vector<entt::entity> navigableControls;

	auto setTopmostRaycastTarget = [&](entt::entity entity, int32_t sortOrder, UIControlKind kind)
		{
			capturedByUI = true;
			if (!hasTopmostRaycastTarget || sortOrder >= hoveredTarget.m_SortOrder)
			{
				hasTopmostRaycastTarget = true;
				hoveredTarget = { entity, kind, sortOrder };
			}
		};

	if (pointer.m_Active)
	{
		auto panelView = m_Registry.view<UITransformComponent, UIPanelComponent>();
		for (auto entity : panelView)
		{
			auto [transform, panel] = panelView.get<UITransformComponent, UIPanelComponent>(entity);
			if (!transform.m_Visible || !panel.m_RaycastTarget || !IsUIBranchVisible(*this, m_Registry, entity))
				continue;

			const UIRect rect = ResolveUIRect(*this, m_Registry, entity, viewportSize);
			if (ContainsPoint(rect, pointer.m_Position))
				setTopmostRaycastTarget(entity, transform.m_SortOrder, UIControlKind::None);
		}

		auto imageView = m_Registry.view<UITransformComponent, UIImageComponent>();
		for (auto entity : imageView)
		{
			auto [transform, image] = imageView.get<UITransformComponent, UIImageComponent>(entity);
			if (!transform.m_Visible || !image.m_RaycastTarget || !IsUIBranchVisible(*this, m_Registry, entity))
				continue;

			const UIRect rect = ResolveUIRect(*this, m_Registry, entity, viewportSize);
			if (ContainsPoint(rect, pointer.m_Position))
				setTopmostRaycastTarget(entity, transform.m_SortOrder, UIControlKind::None);
		}

		for (auto entity : buttonView)
		{
			auto [transform, button] = buttonView.get<UITransformComponent, UIButtonComponent>(entity);
			if (!transform.m_Visible || !button.m_RaycastTarget || !IsUIBranchVisible(*this, m_Registry, entity))
				continue;

			const UIRect rect = ResolveUIRect(*this, m_Registry, entity, viewportSize);
			if (!ContainsPoint(rect, pointer.m_Position))
				continue;

			setTopmostRaycastTarget(entity, transform.m_SortOrder, button.m_Interactable ? UIControlKind::Button : UIControlKind::None);
		}

		for (auto entity : toggleView)
		{
			auto [transform, toggle] = toggleView.get<UITransformComponent, UIToggleComponent>(entity);
			if (!transform.m_Visible || !toggle.m_RaycastTarget || !IsUIBranchVisible(*this, m_Registry, entity))
				continue;

			const UIRect rect = ResolveUIRect(*this, m_Registry, entity, viewportSize);
			if (ContainsPoint(rect, pointer.m_Position))
				setTopmostRaycastTarget(entity, transform.m_SortOrder, toggle.m_Interactable ? UIControlKind::Toggle : UIControlKind::None);
		}

		for (auto entity : sliderView)
		{
			auto [transform, slider] = sliderView.get<UITransformComponent, UISliderComponent>(entity);
			if (!transform.m_Visible || !slider.m_RaycastTarget || !IsUIBranchVisible(*this, m_Registry, entity))
				continue;

			const UIRect rect = ResolveUIRect(*this, m_Registry, entity, viewportSize);
			if (ContainsPoint(rect, pointer.m_Position))
				setTopmostRaycastTarget(entity, transform.m_SortOrder, slider.m_Interactable ? UIControlKind::Slider : UIControlKind::None);
		}

		for (auto entity : inputFieldView)
		{
			auto [transform, inputField] = inputFieldView.get<UITransformComponent, UIInputFieldComponent>(entity);
			if (!transform.m_Visible || !inputField.m_RaycastTarget || !IsUIBranchVisible(*this, m_Registry, entity))
				continue;

			const UIRect rect = ResolveUIRect(*this, m_Registry, entity, viewportSize);
			if (ContainsPoint(rect, pointer.m_Position))
				setTopmostRaycastTarget(entity, transform.m_SortOrder, inputField.m_Interactable ? UIControlKind::InputField : UIControlKind::None);
		}

		for (auto entity : buttonView)
		{
			auto [transform, button] = buttonView.get<UITransformComponent, UIButtonComponent>(entity);
			if (transform.m_Visible && button.m_Interactable && button.m_RaycastTarget && button.m_NavigationEnabled && IsUIBranchVisible(*this, m_Registry, entity))
				navigableControls.push_back(entity);
		}
		for (auto entity : toggleView)
		{
			auto [transform, toggle] = toggleView.get<UITransformComponent, UIToggleComponent>(entity);
			if (transform.m_Visible && toggle.m_Interactable && toggle.m_RaycastTarget && toggle.m_NavigationEnabled && IsUIBranchVisible(*this, m_Registry, entity))
				navigableControls.push_back(entity);
		}
		for (auto entity : sliderView)
		{
			auto [transform, slider] = sliderView.get<UITransformComponent, UISliderComponent>(entity);
			if (transform.m_Visible && slider.m_Interactable && slider.m_RaycastTarget && IsUIBranchVisible(*this, m_Registry, entity))
				navigableControls.push_back(entity);
		}
		for (auto entity : inputFieldView)
		{
			auto [transform, inputField] = inputFieldView.get<UITransformComponent, UIInputFieldComponent>(entity);
			if (transform.m_Visible && inputField.m_Interactable && inputField.m_RaycastTarget && IsUIBranchVisible(*this, m_Registry, entity))
				navigableControls.push_back(entity);
		}
	}

	std::sort(navigableControls.begin(), navigableControls.end(), [&](entt::entity left, entt::entity right)
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

	Entity focusedEntity = m_FocusedUIControl ? FindEntityByUUID(m_FocusedUIControl) : Entity{};
	auto focusedIt = focusedEntity ? std::find(navigableControls.begin(), navigableControls.end(), static_cast<entt::entity>(focusedEntity)) : navigableControls.end();
	if (focusedIt == navigableControls.end())
	{
		m_FocusedUIControl = 0;
		focusedEntity = {};
	}

	if (hoveredTarget.m_Entity != entt::null && pointer.m_Pressed)
	{
		Entity hoveredEntity{ hoveredTarget.m_Entity, this };
		if (hoveredTarget.m_Kind != UIControlKind::None)
			m_FocusedUIControl = hoveredEntity.GetUUID();
		else
			m_FocusedUIControl = 0;
		focusedEntity = hoveredTarget.m_Kind != UIControlKind::None ? hoveredEntity : Entity{};
		focusedIt = focusedEntity ? std::find(navigableControls.begin(), navigableControls.end(), hoveredTarget.m_Entity) : navigableControls.end();
	}

	const bool shiftHeld = Input::IsKeyDown(Key::LeftShift) || Input::IsKeyDown(Key::RightShift);
	const bool nextRequested = (Input::IsKeyPressed(Key::Tab) && !shiftHeld) || Input::IsKeyPressed(Key::Down) || Input::IsKeyPressed(Key::Right);
	const bool previousRequested = (Input::IsKeyPressed(Key::Tab) && shiftHeld) || Input::IsKeyPressed(Key::Up) || Input::IsKeyPressed(Key::Left);
	const bool submitRequested = Input::IsKeyPressed(Key::Enter) || Input::IsKeyPressed(Key::KPEnter) || Input::IsKeyPressed(Key::Space);
	const bool navigationUsed = pointer.m_Active && !navigableControls.empty() && (nextRequested || previousRequested || submitRequested);
	if (navigationUsed)
	{
		capturedByUI = true;
		if (focusedIt == navigableControls.end())
			focusedIt = navigableControls.begin();

		if (nextRequested || previousRequested)
		{
			size_t focusedIndex = static_cast<size_t>(std::distance(navigableControls.begin(), focusedIt));
			if (nextRequested)
				focusedIndex = (focusedIndex + 1) % navigableControls.size();
			else
				focusedIndex = focusedIndex == 0 ? navigableControls.size() - 1 : focusedIndex - 1;
			focusedIt = navigableControls.begin() + static_cast<std::ptrdiff_t>(focusedIndex);
		}

		focusedEntity = Entity{ *focusedIt, this };
		m_FocusedUIControl = focusedEntity.GetUUID();
	}

	Input::SetRuntimeInputCapturedByUI(capturedByUI);

	if (focusedEntity && focusedEntity.HasComponent<UIButtonComponent>())
		focusedEntity.GetComponent<UIButtonComponent>().m_Focused = true;
	if (focusedEntity && focusedEntity.HasComponent<UIToggleComponent>())
		focusedEntity.GetComponent<UIToggleComponent>().m_Focused = true;
	if (focusedEntity && focusedEntity.HasComponent<UISliderComponent>())
		focusedEntity.GetComponent<UISliderComponent>().m_Focused = true;
	if (focusedEntity && focusedEntity.HasComponent<UIInputFieldComponent>())
		focusedEntity.GetComponent<UIInputFieldComponent>().m_Focused = true;
	for (auto entity : inputFieldView)
	{
		if (!focusedEntity || static_cast<entt::entity>(focusedEntity) != entity)
			m_Registry.get<UIInputFieldComponent>(entity).m_Focused = false;
	}

	if (hoveredTarget.m_Entity != entt::null && hoveredTarget.m_Kind == UIControlKind::Button)
	{
		auto& button = m_Registry.get<UIButtonComponent>(hoveredTarget.m_Entity);
		button.m_Hovered = true;
		button.m_Pressed = pointer.m_Down;
		button.m_ClickedThisFrame = pointer.m_Released;
		if (button.m_ClickedThisFrame)
			InvokeUIRuntimeCallback(Entity{ hoveredTarget.m_Entity, this }, EntityMethodType::OnUIClick, button.m_OnClickCallback);
	}

	if (submitRequested && focusedEntity && focusedEntity.HasComponent<UIButtonComponent>() && pointer.m_Active)
	{
		auto& button = focusedEntity.GetComponent<UIButtonComponent>();
		if (button.m_Interactable && button.m_NavigationEnabled)
		{
			button.m_Pressed = true;
			button.m_ClickedThisFrame = true;
			button.m_SubmittedThisFrame = true;
			InvokeUIRuntimeCallback(focusedEntity, EntityMethodType::OnUIClick, button.m_OnClickCallback);
		}
	}

	if (hoveredTarget.m_Entity != entt::null && hoveredTarget.m_Kind == UIControlKind::Toggle)
	{
		auto& toggle = m_Registry.get<UIToggleComponent>(hoveredTarget.m_Entity);
		toggle.m_Hovered = true;
		toggle.m_Pressed = pointer.m_Down;
		if (pointer.m_Released)
		{
			toggle.m_Checked = !toggle.m_Checked;
			toggle.m_ChangedThisFrame = true;
			bool value = toggle.m_Checked;
			InvokeUIRuntimeCallback(Entity{ hoveredTarget.m_Entity, this }, EntityMethodType::OnUIToggle, toggle.m_OnValueChangedCallback, Payload::Ref(value));
		}
	}

	if (submitRequested && focusedEntity && focusedEntity.HasComponent<UIToggleComponent>() && pointer.m_Active)
	{
		auto& toggle = focusedEntity.GetComponent<UIToggleComponent>();
		if (toggle.m_Interactable && toggle.m_NavigationEnabled)
		{
			toggle.m_Checked = !toggle.m_Checked;
			toggle.m_ChangedThisFrame = true;
			bool value = toggle.m_Checked;
			InvokeUIRuntimeCallback(focusedEntity, EntityMethodType::OnUIToggle, toggle.m_OnValueChangedCallback, Payload::Ref(value));
		}
	}

	if (hoveredTarget.m_Entity != entt::null && hoveredTarget.m_Kind == UIControlKind::Slider)
	{
		auto& slider = m_Registry.get<UISliderComponent>(hoveredTarget.m_Entity);
		slider.m_Hovered = true;
		slider.m_Pressed = pointer.m_Down;
		if (pointer.m_Down)
		{
			const UIRect rect = ResolveUIRect(*this, m_Registry, hoveredTarget.m_Entity, viewportSize);
			const float oldValue = slider.m_Value;
			slider.m_Value = ValueFromSliderPoint(slider, rect, pointer.m_Position);
			slider.m_ChangedThisFrame = std::abs(oldValue - slider.m_Value) > 0.0001f;
			if (slider.m_ChangedThisFrame)
			{
				float value = slider.m_Value;
				InvokeUIRuntimeCallback(Entity{ hoveredTarget.m_Entity, this }, EntityMethodType::OnUISlider, slider.m_OnValueChangedCallback, Payload::Ref(value));
			}
		}
	}

	if (focusedEntity && focusedEntity.HasComponent<UISliderComponent>() && pointer.m_Active)
	{
		auto& slider = focusedEntity.GetComponent<UISliderComponent>();
		const float step = std::max(std::abs(slider.m_MaxValue - slider.m_MinValue) * 0.05f, 0.01f);
		const float oldValue = slider.m_Value;
		if (Input::IsKeyPressed(Key::Left))
			slider.m_Value -= step;
		if (Input::IsKeyPressed(Key::Right))
			slider.m_Value += step;
		slider.m_Value = glm::clamp(slider.m_Value, std::min(slider.m_MinValue, slider.m_MaxValue), std::max(slider.m_MinValue, slider.m_MaxValue));
		slider.m_ChangedThisFrame |= std::abs(oldValue - slider.m_Value) > 0.0001f;
		if (slider.m_ChangedThisFrame)
		{
			float value = slider.m_Value;
			InvokeUIRuntimeCallback(focusedEntity, EntityMethodType::OnUISlider, slider.m_OnValueChangedCallback, Payload::Ref(value));
		}
	}

	if (hoveredTarget.m_Entity != entt::null && hoveredTarget.m_Kind == UIControlKind::InputField)
		m_Registry.get<UIInputFieldComponent>(hoveredTarget.m_Entity).m_Hovered = true;

	if (focusedEntity && focusedEntity.HasComponent<UIInputFieldComponent>() && pointer.m_Active)
	{
		auto& inputField = focusedEntity.GetComponent<UIInputFieldComponent>();
		capturedByUI = true;
		const bool changed = AppendRuntimeTextInput(inputField.m_Text, inputField.m_MaxCharacters);
		inputField.m_ChangedThisFrame = changed;
		if (changed)
		{
			std::string_view textView(inputField.m_Text);
			InvokeUIRuntimeCallback(focusedEntity, EntityMethodType::OnUIInputChanged, inputField.m_OnValueChangedCallback, Payload::Ref(textView));
		}
		if (Input::IsKeyPressed(Key::Enter) || Input::IsKeyPressed(Key::KPEnter))
		{
			inputField.m_SubmittedThisFrame = true;
			std::string_view textView(inputField.m_Text);
			InvokeUIRuntimeCallback(focusedEntity, EntityMethodType::OnUIInputSubmit, inputField.m_OnSubmitCallback, Payload::Ref(textView));
		}
		Input::SetRuntimeInputCapturedByUI(true);
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
			std::ranges::reverse(children);

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
			if (transform.m_Visible && IsUIBranchVisible(*this, m_Registry, entity))
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

		if (m_Registry.any_of<UIPanelComponent>(entity))
		{
			const auto& panel = m_Registry.get<UIPanelComponent>(entity);
			Renderer2D::DrawQuad(BuildUITransform(rect, transform, z + 0.0001f), panel.m_Color, entityId);
		}

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
			DrawUIText(button.m_Text, button.m_Font, button.m_TextColor, button.m_FontSize, 0.0f, 0.0f, button.m_TextHorizontalAlignment, button.m_TextVerticalAlignment, rect, z + 0.0005f, entityId);
		}

		if (m_Registry.any_of<UIToggleComponent>(entity))
		{
			const auto& toggle = m_Registry.get<UIToggleComponent>(entity);
			const float boxSize = std::min(rect.m_Size.y * 0.62f, 28.0f);
			UIRect boxRect;
			boxRect.m_Size = { boxSize, boxSize };
			boxRect.m_Center = { rect.m_Min.x + 12.0f + boxSize * 0.5f, rect.m_Center.y };
			boxRect.m_Min = boxRect.m_Center - boxRect.m_Size * 0.5f;
			boxRect.m_Max = boxRect.m_Center + boxRect.m_Size * 0.5f;

			glm::vec4 boxColor = toggle.m_Hovered || toggle.m_Focused ? glm::mix(toggle.m_BoxColor, toggle.m_HoveredColor, 0.6f) : toggle.m_BoxColor;
			if (!toggle.m_Interactable)
				boxColor.a *= 0.55f;

			Renderer2D::DrawQuad(BuildUITransform(boxRect, transform, z + 0.00025f), boxColor, entityId);
			Renderer2D::DrawRect(BuildUITransform(boxRect, transform, z + 0.00035f), toggle.m_CheckColor, entityId);
			if (toggle.m_Checked)
			{
				UIRect checkRect = boxRect;
				checkRect.m_Size = glm::max(boxRect.m_Size - glm::vec2(10.0f), glm::vec2(4.0f));
				Renderer2D::DrawQuad(BuildUITransform(checkRect, transform, z + 0.00045f), toggle.m_CheckColor, entityId);
			}

			UIRect labelRect = rect;
			labelRect.m_Min.x = boxRect.m_Max.x + 10.0f;
			labelRect.m_Size.x = std::max(rect.m_Max.x - labelRect.m_Min.x, 1.0f);
			labelRect.m_Center.x = labelRect.m_Min.x + labelRect.m_Size.x * 0.5f;
			DrawUIText(toggle.m_Label, toggle.m_Font, toggle.m_TextColor, toggle.m_FontSize, 0.0f, 0.0f, UITextHorizontalAlignment::Left, UITextVerticalAlignment::Center, labelRect, z + 0.00055f, entityId);
		}

		if (m_Registry.any_of<UISliderComponent>(entity))
		{
			const auto& slider = m_Registry.get<UISliderComponent>(entity);
			const float trackHeight = std::min(std::max(rect.m_Size.y * 0.22f, 6.0f), 14.0f);
			UIRect trackRect = rect;
			trackRect.m_Size.y = trackHeight;
			trackRect.m_Center.y = rect.m_Center.y;
			trackRect.m_Min.y = trackRect.m_Center.y - trackHeight * 0.5f;
			trackRect.m_Max.y = trackRect.m_Center.y + trackHeight * 0.5f;
			Renderer2D::DrawQuad(BuildUITransform(trackRect, transform, z + 0.00025f), slider.m_BackgroundColor, entityId);

			const float normalized = NormalizeSliderValue(slider);
			UIRect fillRect = trackRect;
			fillRect.m_Size.x = std::max(trackRect.m_Size.x * normalized, 1.0f);
			fillRect.m_Center.x = trackRect.m_Min.x + fillRect.m_Size.x * 0.5f;
			fillRect.m_Max.x = fillRect.m_Min.x + fillRect.m_Size.x;
			Renderer2D::DrawQuad(BuildUITransform(fillRect, transform, z + 0.00035f), slider.m_FillColor, entityId);

			UIRect handleRect;
			const float handleSize = std::min(std::max(rect.m_Size.y * 0.55f, 18.0f), 34.0f);
			handleRect.m_Size = { handleSize, handleSize };
			handleRect.m_Center = { glm::mix(trackRect.m_Min.x, trackRect.m_Max.x, normalized), trackRect.m_Center.y };
			handleRect.m_Min = handleRect.m_Center - handleRect.m_Size * 0.5f;
			handleRect.m_Max = handleRect.m_Center + handleRect.m_Size * 0.5f;
			glm::vec4 handleColor = slider.m_Hovered || slider.m_Focused ? glm::mix(slider.m_HandleColor, slider.m_FillColor, 0.25f) : slider.m_HandleColor;
			Renderer2D::DrawQuad(BuildUITransform(handleRect, transform, z + 0.0005f), handleColor, entityId);
		}

		if (m_Registry.any_of<UIInputFieldComponent>(entity))
		{
			const auto& inputField = m_Registry.get<UIInputFieldComponent>(entity);
			const glm::vec4 backgroundColor = inputField.m_Focused ? inputField.m_FocusedColor : inputField.m_BackgroundColor;
			Renderer2D::DrawQuad(BuildUITransform(rect, transform, z + 0.00025f), backgroundColor, entityId);
			if (inputField.m_Focused || inputField.m_Hovered)
				Renderer2D::DrawRect(BuildUITransform(rect, transform, z + 0.00035f), glm::vec4(0.35f, 0.62f, 0.88f, 0.95f), entityId);

			const bool hasText = !inputField.m_Text.empty();
			DrawUIText(hasText ? inputField.m_Text : inputField.m_Placeholder,
				inputField.m_Font,
				hasText ? inputField.m_TextColor : inputField.m_PlaceholderColor,
				inputField.m_FontSize,
				0.0f,
				0.0f,
				UITextHorizontalAlignment::Left,
				UITextVerticalAlignment::Center,
				rect,
				z + 0.00055f,
				entityId);
		}

		if (m_Registry.any_of<UITextComponent>(entity))
		{
			const auto& text = m_Registry.get<UITextComponent>(entity);
			DrawUIText(text.m_TextString, text.m_Font, text.m_Color, text.m_FontSize, text.m_Kerning, text.m_LineSpacing, text.m_HorizontalAlignment, text.m_VerticalAlignment, rect, z + 0.00075f, entityId);
		}
	}

	Renderer2D::EndScene();
	RenderCommand::SetDepthTest(true);
}

void Scene::RenderUIOverlayDebug(const std::vector<UUID>& selectedEntities)
{
	WHP_PROFILE_FUNCTION();
	if (m_ViewportWidth == 0 || m_ViewportHeight == 0)
		return;

	const glm::vec2 viewportSize{ static_cast<float>(m_ViewportWidth), static_cast<float>(m_ViewportHeight) };
	OrthographicCamera uiCamera(0.0f, viewportSize.x, 0.0f, viewportSize.y);
	RenderCommand::SetDepthTest(false);
	Renderer2D::BeginScene(uiCamera);

	auto canvasView = m_Registry.view<UITransformComponent, UICanvasComponent>();
	for (auto entity : canvasView)
	{
		const auto& canvas = m_Registry.get<UICanvasComponent>(entity);
		if (!canvas.m_ShowInEditor || !canvas.m_Visible || !IsUIBranchVisible(*this, m_Registry, entity))
			continue;

		const auto& transform = m_Registry.get<UITransformComponent>(entity);
		const UIRect rect = ResolveUIRect(*this, m_Registry, entity, viewportSize);
		Renderer2D::DrawRect(BuildUITransform(rect, transform, -0.01f), glm::vec4(0.32f, 0.62f, 0.85f, 0.72f));
		if (canvas.m_ShowSafeAreaInEditor)
		{
			const glm::vec4 insets = glm::clamp(canvas.m_SafeAreaInsets, glm::vec4(0.0f), glm::vec4(0.45f));
			UIRect safeRect = rect;
			safeRect.m_Min.x += rect.m_Size.x * insets.x;
			safeRect.m_Max.x -= rect.m_Size.x * insets.z;
			safeRect.m_Max.y -= rect.m_Size.y * insets.y;
			safeRect.m_Min.y += rect.m_Size.y * insets.w;
			safeRect.m_Size = glm::max(safeRect.m_Max - safeRect.m_Min, glm::vec2(1.0f));
			safeRect.m_Center = safeRect.m_Min + safeRect.m_Size * 0.5f;
			Renderer2D::DrawRect(BuildUITransform(safeRect, transform, -0.009f), glm::vec4(0.38f, 0.88f, 0.70f, 0.86f));
		}
	}

	for (UUID selectedId : selectedEntities)
	{
		Entity selected = FindEntityByUUID(selectedId);
		if (!selected || !selected.HasComponent<UITransformComponent>() || !IsUIBranchVisible(*this, m_Registry, static_cast<entt::entity>(selected)))
			continue;

		const auto& transform = selected.GetComponent<UITransformComponent>();
		const UIRect rect = ResolveUIRect(*this, m_Registry, static_cast<entt::entity>(selected), viewportSize);
		Renderer2D::DrawRect(BuildUITransform(rect, transform, -0.005f), glm::vec4(0.95f, 0.55f, 0.16f, 1.0f));
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
void Scene::OnComponentAdded<UICanvasComponent>(Entity entityIn, UICanvasComponent& component)
{
}

template<>
void Scene::OnComponentAdded<UIPanelComponent>(Entity entityIn, UIPanelComponent& component)
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
void Scene::OnComponentAdded<UIToggleComponent>(Entity entityIn, UIToggleComponent& component)
{
}

template<>
void Scene::OnComponentAdded<UISliderComponent>(Entity entityIn, UISliderComponent& component)
{
}

template<>
void Scene::OnComponentAdded<UIInputFieldComponent>(Entity entityIn, UIInputFieldComponent& component)
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
