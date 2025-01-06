#include "whippch.h"

#include "entity.h"

_WHIP_START

entity::entity(entt::entity handle, scene* scene) : m_entity_handle(handle), m_scene(scene) {}

_WHIP_END