#include "WhipPch.h"

#include <Whip/Scene/Entity.h>

_WHIP_START

Entity::Entity(entt::entity handle, Scene* scene) : m_EntityHandle(handle), m_Scene(scene) {}

_WHIP_END
