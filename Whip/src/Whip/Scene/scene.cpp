#include "whippch.h"
#include "scene.h"

#include "components.h"
#include "scriptable_entity.h"

#include <Whip/Core/Input.h>
#include <Whip/Scripting/script_engine.h>
#include <Whip/Render/Renderer2D.h>

#include <Whip/Physics/physics2D.h>
#include <Whip/Physics/physics_world.h>

#include <Whip/Project/project.h>
#include <Whip/Audio/audio_engine.h>
#include <Whip/Animation/animation_manager.h>

#include <glm/glm.hpp>

#include <coco.h>

#include "entity.h"

#include "box2d/b2_world.h"
#include "box2d/b2_body.h"
#include "box2d/b2_fixture.h"
#include "box2d/b2_polygon_shape.h"
#include "box2d/b2_circle_shape.h"

_WHIP_START

namespace utils
{
	template<class... Components>
	static void copy_component(entt::registry& dst, entt::registry& src, const std::unordered_map<UUID, entt::entity>& entt_map)
	{
		([&]()
			{
				auto view = src.view<Components>();
				for (auto src_entity : view)
				{
					entt::entity dstEntity = entt_map.at(src.get<ID_component>(src_entity).ID);

					auto& src_component = src.get<Components>(src_entity);
					dst.emplace_or_replace<Components>(dstEntity, src_component);
				}
			}(), ...);
	}

	template<class... Components>
	static void copy_component(component_group<Components...>, entt::registry& dst, entt::registry& src, const std::unordered_map<UUID, entt::entity>& entt_map)
	{
		copy_component<Components...>(dst, src, entt_map);
	}

	template<class... Components>
	static void copy_component_if_exist(entity dst, entity src)
	{
		([&]()
			{
				if (src.has_component<Components>())
					dst.add_or_replace_component<Components>(src.get_component<Components>());
			}(), ...);
	}

	template<class... Components>
	static void copy_component_if_exist(component_group<Components...>, entity dst, entity src)
	{
		copy_component_if_exist<Components...>(dst, src);
	}
}

scene::scene(asset_handle handle) : asset(handle) 
{
	m_physics_world.set_scene_context(this);
}

scene::~scene() {}

ref<scene> scene::copy(ref<scene> other)
{
	WHP_PROFILE_FUNCTION();
	ref<scene> new_scene = make_ref<scene>();

	new_scene->m_viewport_width = other->m_viewport_width;
	new_scene->m_viewport_height = other->m_viewport_height;

	auto& src_scene_registry = other->m_registry;
	auto& dst_scene_registry = new_scene->m_registry;
	std::unordered_map<UUID, entt::entity> entt_map;

	auto id_view = src_scene_registry.view<ID_component>();
	for (auto e : id_view)
	{
		UUID uuid = src_scene_registry.get<ID_component>(e).ID;
		const auto& name = src_scene_registry.get<tag_component>(e).tag;
		entity new_entity = new_scene->create_entity_with_UUID(uuid, name);
		entt_map[uuid] = (entt::entity)new_entity;
	}

	utils::copy_component(all_components_no_id_no_tag{}, dst_scene_registry, src_scene_registry, entt_map);

	return new_scene;
}

entity scene::create_entity(const std::string& name)
{
	return create_entity_with_UUID(UUID(), name);
}

entity scene::create_entity_with_UUID(UUID uuid, const std::string& name)
{
	entity result = { m_registry.create(), this };
	result.add_component<ID_component>(ID_component(uuid));
	result.add_component<transform_component>();
	auto& tag = result.add_component<tag_component>();
	tag.tag = m_unique_name_manager.add_name(name);
	m_entity_map[uuid] = result;
	return result;
}

void scene::destroy_entity(entity entity_in)
{
	m_unique_name_manager.remove_name(entity_in.get_name());
	m_entity_map.erase(entity_in.get_UUID());
    m_registry.destroy(entity_in);
}

entity scene::find_entity_by_UUID(UUID uuid)
{
	auto it = m_entity_map.find(uuid);
	if (it != m_entity_map.end())
		return { it->second, this };
	return entity{};
}

entity scene::find_entity_by_name(std::string_view name)
{
	auto view = m_registry.view<tag_component>();
	for (auto entityID : view)
	{
		const tag_component& tag_comp = view.get<tag_component>(entityID);
		if (tag_comp.tag == name)
			return entity{ entityID, this };
	}
	return entity{};
}

void scene::on_runtime_start()
{
	WHP_PROFILE_FUNCTION();
	m_is_running = true;
	m_physics_world.create();
	{
		script_engine::on_runtime_start(this);
		auto view = m_registry.view<script_component>();
		for (auto e : view)
		{
			entity ent = { e, this };
			script_engine::on_create_entity(ent);
		}
		script_engine::invoke_all_on_create_methods();
	}
}

void scene::on_runtime_stop()
{
	WHP_PROFILE_FUNCTION();
	m_is_running = false;
	m_physics_world.destroy();
	script_engine::on_runtime_stop();
	on_audios_stop();
}

void scene::on_simulation_start()
{
	WHP_PROFILE_FUNCTION();
	script_engine::on_runtime_start(this);
	m_physics_world.create();
}

void scene::on_simulation_stop()
{
	WHP_PROFILE_FUNCTION();
	m_physics_world.destroy();
	on_audios_stop();
}

void scene::on_update_runtime(timestep ts)
{
	WHP_PROFILE_FUNCTION();
	if(!m_is_paused || m_step_frames-- > 0)
	{
		{
			{
				// C# OnUpdate
				auto view = m_registry.view<script_component>();
				for (auto e : view)
				{
					entity ent = { e, this };
					script_engine::on_update_entity(ent, ts);
				}
			}

			m_registry.view<native_script_component>().each([=](auto ent, auto& nsc)
				{
					if (!nsc.instance)
					{
						nsc.instance = nsc.instantiate_script();
						nsc.instance->m_entity = entity{ ent, this };
						nsc.instance->on_create();
					}
					nsc.instance->on_update(ts);
				});
		}

		// Physics
		{
			m_physics_world.update(ts);
		}

		// animations
		{
			animation_manager::get().update(ts);
		}
	}

    camera* main_camera = nullptr;
    glm::mat4 camera_transform;
    {
        auto group = m_registry.group<transform_component>(entt::get<camera_component>);
        for (auto entity : group)
        {
            auto [transform, cam] = group.get<transform_component, camera_component>(entity);
            if (cam.primary)
            {
                main_camera = &cam.camera;
                camera_transform = transform.get_transform();
                break;
            }
        }
    }
    if (main_camera)
    {
		// sprites
        renderer2D::begin_scene(*main_camera, camera_transform);
        {
			auto view = m_registry.view<transform_component, sprite_renderer_component>();
			for (auto ent : view)
			{
				const auto& [transform, sprite] = view.get<transform_component, sprite_renderer_component>(ent);
				renderer2D::draw_sprite(transform.get_transform(), sprite, (int)ent);
			}
        }

		// circles
		{
			auto view = m_registry.view<transform_component, circle_renderer_component>();
			for (auto ent : view)
			{
				const auto& [transform, circle] = view.get<transform_component, circle_renderer_component>(ent);
			
				renderer2D::draw_circle(transform.get_transform(), circle.color, circle.thickness, circle.fade, (int)ent);
			}
		}


		// texts
		{
			auto view = m_registry.view<transform_component, text_component>();
			for (auto entity : view)
			{
				const auto& [transform, text] = view.get<transform_component, text_component>(entity);

				renderer2D::draw_string(text.text_string, transform.get_transform(), text, (int)entity);
			}
		}

        renderer2D::end_scene();
    }
}

void scene::on_update_simulation(timestep ts, editor_camera& cam)
{
	if (!m_is_paused || m_step_frames-- > 0)
	{
		{
			// C# OnUpdate
			auto view = m_registry.view<script_component>();
			for (auto e : view)
			{
				entity ent = { e, this };
				script_engine::on_update_entity(ent, ts);
			}
		}
		
		m_physics_world.update(ts);
		animation_manager::get().update(ts);
	}

	render_scene(cam);
}

void scene::on_update_editor(timestep ts, editor_camera& cam)
{
	WHP_PROFILE_FUNCTION();
	render_scene(cam);
}

void scene::on_viewport_resize(uint32_t width, uint32_t height)
{
	WHP_PROFILE_FUNCTION();
    m_viewport_width = width;
    m_viewport_height = height;

    auto group = m_registry.group<camera_component>();
    for (auto entity : group)
    {
        auto& component = group.get<camera_component>(entity);
        if (!component.fixed_aspect_ratio)
            component.camera.set_viewport_size(width, height);
    }
}

entity scene::duplicate_entity(entity entity_in)
{
	WHP_PROFILE_FUNCTION();
	std::string name = entity_in.get_name();
	entity new_entity = create_entity(name);

	utils::copy_component_if_exist(all_components_no_id_no_tag{}, new_entity, entity_in);
	return new_entity;
}

entity scene::get_primary_camera_entity()
{
	WHP_PROFILE_FUNCTION();
    auto view = m_registry.view<camera_component>();
    for (auto ent : view)
    {
        const auto& camera = view.get<camera_component>(ent);
        if (camera.primary)
            return entity{ ent, this };
    }
    return {};
}

void scene::step(int frames)
{
	m_step_frames = frames;
}

void scene::on_audios_stop()
{
	auto view = m_registry.view<audio_component>();
	for (auto e : view)
	{
		const auto& ac = view.get<audio_component>(e);
		for (const auto& audio_handle : ac.audio_datas)
		{
			auto asset = project::get_active()->get_asset_manager()->get_asset(audio_handle.audio);
			auto audio_asset = std::static_pointer_cast<audio_source>(asset);
			audio_engine::stop(audio_asset);
		}
	}
}

void scene::render_scene(editor_camera& cam)
{
	WHP_PROFILE_FUNCTION();
	renderer2D::begin_scene(cam);

	// sprite
	{
		auto view = m_registry.view<transform_component, sprite_renderer_component>();
		for (auto entity : view)
		{
			const auto& [transform, sprite] = view.get<transform_component, sprite_renderer_component>(entity);

			renderer2D::draw_sprite(transform.get_transform(), sprite, (int)entity);
		}
	}

	// circle
	{
		auto view = m_registry.view<transform_component, circle_renderer_component>();
		for (auto entity : view)
		{
			const auto& [transform, circle] = view.get<transform_component, circle_renderer_component>(entity);

			renderer2D::draw_circle(transform.get_transform(), circle.color, circle.thickness, circle.fade, (int)entity);
		}
	}

	// texts
	{
		auto view = m_registry.view<transform_component, text_component>();
		for (auto entity : view)
		{
			const auto& [transform, text] = view.get<transform_component, text_component>(entity);

			renderer2D::draw_string(text.text_string, transform.get_transform(), text, (int)entity);
		}
	}

	renderer2D::end_scene();
}

template<>
void scene::on_component_added<transform_component>(entity entity_in, transform_component& component)
{
}

template<>
void scene::on_component_added<camera_component>(entity entity_in, camera_component& component)
{
	if(m_viewport_width > 0 && m_viewport_height > 0)
		component.camera.set_viewport_size(m_viewport_width, m_viewport_height);
}

template<>
void scene::on_component_added<sprite_renderer_component>(entity entity_in, sprite_renderer_component& component)
{
}

template<>
void scene::on_component_added<circle_renderer_component>(entity entity_in, circle_renderer_component& component)
{
}

template<>
void scene::on_component_added<text_component>(entity entity_in, text_component& component)
{
}

template<>
void scene::on_component_added<tag_component>(entity entity_in, tag_component& component)
{
}

template<>
void scene::on_component_added<script_component>(entity entity_in, script_component& component)
{
}

template<>
void scene::on_component_added<native_script_component>(entity entity_in, native_script_component& component)
{
}

template<>
void scene::on_component_added<rigidbody2D_component>(entity entity_in, rigidbody2D_component& component)
{
}

template<>
void scene::on_component_added<box_collider2D_component>(entity entity_in, box_collider2D_component& component)
{
}

template<>
void scene::on_component_added<circle_collider2D_component>(entity entity_in, circle_collider2D_component& component)
{
}

template<>
void scene::on_component_added<ID_component>(entity entity_in, ID_component& component)
{
}

template<>
void scene::on_component_added<audio_component>(entity entity_in, audio_component& component)
{
}

_WHIP_END
