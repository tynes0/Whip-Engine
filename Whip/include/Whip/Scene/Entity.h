#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/Log.h>
#include <Whip/Core/UUID.h>

#include "Scene.h"
#include "Components.h"

#include "entt.hpp"

_WHIP_START


class Entity
{
public:
	Entity() = default;
	Entity(entt::entity handle, Scene* scene);
	Entity(const Entity& other) = default;
	Entity& operator=(const Entity& other) = default;

	template <class T, class... Args>
	T& AddComponent(Args&&... args)
	{
		WHP_CORE_ASSERT(!HasComponent<T>(), "entity already has component!");
		T& component = m_Scene->m_Registry.emplace<T>(m_EntityHandle, std::forward<Args>(args)...);
		m_Scene->OnComponentAdded<T>(*this, component);
		return component;
	}

	template <class T, class... Args>
	T& AddOrReplaceComponent(Args&&... args)
	{
		T& component = m_Scene->m_Registry.emplace_or_replace<T>(m_EntityHandle, std::forward<Args>(args)...);
		m_Scene->OnComponentAdded<T>(*this, component);
		return component;
	}

	template <class T>
	T& GetComponent()
	{
		WHP_CORE_ASSERT(HasComponent<T>(), "entity does not have component!");
		return m_Scene->m_Registry.get<T>(m_EntityHandle);
	}

	template <class T>
	const T& GetComponent() const
	{
		WHP_CORE_ASSERT(HasComponent<T>(), "entity does not have component!");
		return m_Scene->m_Registry.get<T>(m_EntityHandle);
	}

	template <class T>
	bool HasComponent() const
	{
		return m_Scene->m_Registry.any_of<T>(m_EntityHandle);
	}

	template <class T>
	void RemoveComponent() const
	{
		WHP_CORE_ASSERT(HasComponent<T>(), "entity does not have component!");
		m_Scene->m_Registry.remove<T>(m_EntityHandle);
	}

	operator bool() const { return m_EntityHandle != entt::null; }
	operator uint32_t () const { return static_cast<uint32_t>(m_EntityHandle); }
	operator entt::entity() const { return m_EntityHandle; }

	UUID GetUUID() const { return GetComponent<IDComponent>().m_ID; }
	Scene* GetScene() const { return m_Scene; }
	std::string GetName()
	{
		if (m_EntityHandle == entt::null)
			return std::string{"Empty"};
		return GetComponent<TagComponent>().m_Tag;
	}

	bool operator==(const Entity& other) const
	{
		return m_EntityHandle == other.m_EntityHandle && m_Scene == other.m_Scene;
	}

	bool operator!=(const Entity& other) const
	{
		return !(*this == other);
	}

private:
	entt::entity m_EntityHandle{ entt::null };
	Scene* m_Scene = nullptr;
};

_WHIP_END
