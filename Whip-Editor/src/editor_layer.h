#pragma once

#include <Whip.h>
#include <Whip/UI/UI_project.h>
#include <Whip/UI/UI_settings.h>
#include <Whip/UI/UI_statistics.h>
#include <Whip/Render/editor_camera.h>
#include <Whip/Audio/audio_engine.h>

#include "panels/scene_hierarchy_panel.h"
#include "panels/content_browser_panel.h"
#include "panels/animation_editor_panel.h"
#include "panels/console_panel.h"

// TODOLIST
// - entity asset
// - Spawn and destroy entity -> cs
// - add and destroy component (runtime) -> actually i dont think this is necessary
// - field arrays.
// - scene hierararch -> update properties panel -> all of them will be table
// - scene settings
// - serialize runtime
// - add new project popup
// - all the project settings
// - fix font asset
// - fix content_browser_panel asset tree
// - symetric content_browser_panel settings
// - fix animation editor drag drop size
// - there is an issue with scene_hierarchy_panel::draw_component (i guess...)
// - texture manager -> g_icons with this

_WHIP_START

class editor_layer : public layer
{
public:
	editor_layer();
	virtual ~editor_layer() = default;

	virtual void on_attach() override;
	virtual void on_detach() override;
	virtual void on_update(timestep ts) override;
	virtual void on_imgui_render() override;
	virtual void on_event(event& evnt) override;
private:
	bool on_key_pressed(key_pressed_event& evnt);
	bool on_mouse_button_pressed(mouse_button_pressed_event& evnt);
	bool on_window_drop(window_drop_event& evnt);

	void on_overlay_render();

	void new_project();
	void save_project();
	void finish_project_settings();

	bool open_project();
	bool open_project(const std::filesystem::path& path);

	void new_scene();
	void open_scene(asset_handle handle);
	void close_scene();
	void save_scene();
	void save_scene_as();

	void reload_assembly(bool reset_app_assembly_filepath = true) const;

	void serialize_scene(ref<scene> scene_in, const std::filesystem::path& path);

	void on_scene_play();
	void on_scene_simulate();
	void on_scene_stop();
	void on_scene_pause();

	void on_duplicated_entity();
	void on_deleted_entity();

	void UI_toolbar();
private:
	enum class scane_state
	{
		edit = 0,
		play = 1,
		simulate = 2
	};

	timestep m_ts;

	// camera
	editor_camera m_editor_camera;

	// viewport
	glm::vec2 m_viewport_bounds[2]{};
	glm::vec2 m_viewport_size = { 1.0f, 1.0f };
	bool m_viewport_hovered = false;
	bool m_viewport_focused = false;

	// entity
	entity m_hovered_entity;
	entity m_last_selected_entity;

	// UI's
	UI::UI_project m_UI_project;
	UI::UI_settings m_UI_settings;
	UI::UI_statistics m_UI_statistics;
	UI::popup_handler m_popup_handler;

	// scene
	ref<scene> m_active_scene;
	ref<scene> m_editor_scene;
	std::filesystem::path m_editor_scene_path;

	// framebuffer
	ref<framebuffer> m_framebuffer;

	// gizmo
	int m_gizmo_type = 0;

	// states
	scane_state m_scane_state = scane_state::edit;

	// panels
	scene_hierarchy_panel m_scene_hierarchy_panel;
	animation_editor_panel m_animation_editor_panel;
	consol_panel m_console;
	scope<content_browser_panel> m_content_browser_panel;

	// icons
	ref<texture2D> m_play_icon, m_simulate_icon, m_stop_icon, m_pause_icon, m_step_icon;

	ref<audio_source> m_audio_src;
};

_WHIP_END
