#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Scene/scene.h>
#include <Whip/Scene/entity.h>
#include <Whip/Core/memory.h>

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

	entity get_selected_entity() const { return m_selection_context; }
	std::vector<entity> get_selected_entities() const;
	void set_selected_entity(entity entity_in, bool append = false);
	void clear_selection();
private:
	void draw_entity_node(entity entity_in);
	void draw_components(entity entity_in);
	void set_entity_parent(entity child, entity parent);
	bool can_parent_entity(entity child, entity parent) const;
	bool is_descendant_of(entity entity_in, UUID ancestor_id) const;
	void destroy_entity_with_selection(entity entity_in);
	bool is_selected(entity entity_in) const;

	template <class T>
	void display_add_component_entry(const std::string& entry_name);
private:
	ref<scene> m_context;
	entity m_selection_context;
	std::vector<UUID> m_selection_contexts;
};

_WHIP_END
