#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/memory.h>
#include <Whip/Core/Timestep.h>
#include <Whip/Core/UUID.h>
#include <Whip/Core/unique_name_manager.h>
#include <Whip/Asset/asset.h>
#include <Whip/Render/editor_camera.h>

#include <Whip/Physics/physics_world.h>

_WHIP_START

class entity;

class scene : public asset
{
public:
	scene(asset_handle handle = asset_handle{});
	~scene();

	static ref<scene> copy(ref<scene> other);

	virtual asset_type get_type() const override { return asset_type::scene; }

	void on_simulation_start();
	void on_simulation_stop();

	void on_update_runtime(timestep ts);
	void on_update_simulation(timestep ts, editor_camera& cam);
	void on_update_editor(timestep ts, editor_camera& cam);
	void on_viewport_resize(uint32_t width, uint32_t height);

	entity duplicate_entity(entity entity_in);
	entity get_primary_camera_entity();

	bool is_running() const { return m_is_running; }
	bool is_paused() const { return m_is_paused; }

	void set_paused(bool paused) { m_is_paused = paused; }

	void step(int frames = 1);

	template <class... Components>
	auto get_all_entities_with()
	{
		return m_registry.view<Components...>();
	}

	entity create_entity(const std::string& name = std::string());
	entity create_entity_with_UUID(UUID uuid, const std::string& name = std::string());
	void destroy_entity(entity entity_in);
	entity find_entity_by_UUID(UUID id);
	entity find_entity_by_name(std::string_view name);

	void on_runtime_start();
	void on_runtime_stop();
private:

	void on_audios_stop();

	void render_scene(editor_camera& cam);

	template<class T>
	void on_component_added(entity entity_in, T& component);
private:
	friend class entity;
	friend class scene_serializer;
	friend class scene_hierarchy_panel;

	entt::registry m_registry;
	uint32_t m_viewport_width = 0;
	uint32_t m_viewport_height = 0;
	std::unordered_map<UUID, entt::entity> m_entity_map;
	unique_name_manager m_unique_name_manager;

	bool m_is_running = false;
	bool m_is_paused = false;
	int m_step_frames = 0;

	physics_world m_physics_world;
};

_WHIP_END
