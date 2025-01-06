#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/Timestep.h>

#include <entt.hpp>

class b2World;

_WHIP_START

class scene;

class physics_world
{
public:
	physics_world() {}
	physics_world(scene* scene_context) : m_scene_context(scene_context) {}
	~physics_world();

	void set_scene_context(scene* scene_context);

	void create(float gravity_x = 0.0f, float gravity_y = 9.8f);
	void update(timestep ts);
	void destroy();
private:
	bool private_check(bool check_world) const;

	b2World* m_physics_world = nullptr;
	scene* m_scene_context = nullptr;
};

_WHIP_END
