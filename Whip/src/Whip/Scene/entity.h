#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/Log.h>
#include <Whip/Core/UUID.h>

#include "scene.h"
#include "components.h"

#include "entt.hpp"

_WHIP_START


class entity
{
public:
	entity() = default;
	entity(entt::entity handle, scene* scene);
	entity(const entity& other) = default;
	entity& operator=(const entity& other) = default;

	template <class T, class... Args>
	T& add_component(Args&&... args)
	{
		WHP_CORE_ASSERT(!has_component<T>(), "entity already has component!");
		T& component = m_scene->m_registry.emplace<T>(m_entity_handle, std::forward<Args>(args)...);
		m_scene->on_component_added<T>(*this, component);
		return component;
	}

	template <class T, class... Args>
	T& add_or_replace_component(Args&&... args)
	{
		T& component = m_scene->m_registry.emplace_or_replace<T>(m_entity_handle, std::forward<Args>(args)...);
		m_scene->on_component_added<T>(*this, component);
		return component;
	}

	template <class T>
	T& get_component()
	{
		WHP_CORE_ASSERT(has_component<T>(), "entity does not have component!");
		return m_scene->m_registry.get<T>(m_entity_handle);
	}

	template <class T>
	bool has_component()
	{
		return m_scene->m_registry.any_of<T>(m_entity_handle);
	}

	template <class T>
	void remove_component()
	{
		WHP_CORE_ASSERT(has_component<T>(), "entity does not have component!");
		m_scene->m_registry.remove<T>(m_entity_handle);
	}

	operator bool() const { return m_entity_handle != entt::null; }
	operator uint32_t () const { return static_cast<uint32_t>(m_entity_handle); }
	operator entt::entity() const { return m_entity_handle; }

	UUID get_UUID() { return get_component<ID_component>().ID; }
	std::string get_name() 
	{
		if (m_entity_handle == entt::null)
			return std::string{"Empty"};
		return get_component<tag_component>().tag; 
	}

	bool operator==(const entity& other) const
	{
		return m_entity_handle == other.m_entity_handle && m_scene == other.m_scene;
	}

	bool operator!=(const entity& other) const
	{
		return !(*this == other);
	}

private:
	entt::entity m_entity_handle{ entt::null };
	scene* m_scene = nullptr;
};


_WHIP_END
