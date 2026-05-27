#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Scene/scene.h>
#include <Whip/Scene/entity.h>
#include <Whip/Core/memory.h>

#include <functional>
#include <vector>


_WHIP_START

class scene_hierarchy_panel
{
public:
	scene_hierarchy_panel() = default;
	scene_hierarchy_panel(const ref<scene> context);

	void set_context(const ref<scene>& context);
	ref<scene>& get_context() { return m_context; }

	void on_imgui_render();
	void set_scene_change_callback(std::function<void()> callback) { m_scene_change_callback = std::move(callback); }
	void set_open(bool open);
	bool is_open() const { return m_open; }
	bool consume_open_dirty();

	entity get_selected_entity() const { return m_selection_context; }
	std::vector<entity> get_selected_entities() const;
	std::vector<UUID> get_selected_entity_ids() const { return m_selection_contexts; }
	void set_selected_entity(entity entity_in, bool append = false);
	void set_selected_entity_ids(const std::vector<UUID>& ids);
	void select_all();
	void clear_selection();
private:
	void draw_entity_node(entity entity_in);
	void draw_components(entity entity_in);
	void draw_multi_edit_components(const std::vector<entity>& selected_entities);
	void draw_multi_shared_components(const std::vector<entity>& selected_entities);
	void draw_multi_camera_component(const std::vector<entity>& selected_entities);
	void draw_multi_script_component(const std::vector<entity>& selected_entities);
	void draw_multi_sprite_renderer_component(const std::vector<entity>& selected_entities);
	void draw_multi_circle_renderer_component(const std::vector<entity>& selected_entities);
	void draw_multi_text_component(const std::vector<entity>& selected_entities);
	void draw_multi_rigidbody2D_component(const std::vector<entity>& selected_entities);
	void draw_multi_box_collider2D_component(const std::vector<entity>& selected_entities);
	void draw_multi_circle_collider2D_component(const std::vector<entity>& selected_entities);
	void set_entity_parent(entity child, entity parent);
	bool can_parent_entity(entity child, entity parent) const;
	bool is_descendant_of(entity entity_in, UUID ancestor_id) const;
	void destroy_entity_with_selection(entity entity_in);
	bool is_selected(entity entity_in) const;
	void notify_scene_change();
	void begin_property_edit_history();
	void track_property_edit_history();

	template <class T>
	void display_add_component_entry(const std::string& entry_name);
	template <class T>
	size_t count_selected_with_component() const;
	template <class T>
	void add_component_to_selection();
	template <class T>
	void remove_component_from_selection();
	template <class T>
	void draw_multi_component_summary(const char* name, size_t selected_count);
private:
	ref<scene> m_context;
	entity m_selection_context;
	std::vector<UUID> m_selection_contexts;
	std::function<void()> m_scene_change_callback;
	bool m_property_edit_history_active = false;
	bool m_open = true;
	bool m_open_dirty = false;
};

_WHIP_END
