#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Scene/scene.h>
#include <Whip/Scene/entity.h>
#include <Whip/Core/memory.h>
#include <Whip/Events/KeyEvent.h>


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
	void set_selected_entity(entity entity_in);
private:
	void draw_entity_node(entity entity_in);
	void draw_components(entity entity_in);

	template <class T>
	void display_add_component_entry(const std::string& entry_name);
private:
	ref<scene> m_context;
	entity m_selection_context;
};

_WHIP_END
