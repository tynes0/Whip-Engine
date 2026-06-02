#pragma once

#include <Whip.h>
#include <Whip/UI/UI_project_loader.h>
#include <Whip/UI/UI_project.h>
#include <Whip/UI/UI_settings.h>
#include <Whip/UI/UI_statistics.h>
#include <Whip/Render/editor_camera.h>
#include <Whip/Audio/audio_engine.h>

#include "panels/scene_hierarchy_panel.h"
#include "panels/content_browser_panel.h"
#include "panels/animation_editor_panel.h"
#include "panels/console_panel.h"

#include <FileWatch.h>

#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

// TODOLIST
// - entity asset
// - Spawn and destroy entity -> cs
// - add and destroy component (runtime) -> actually I don't think this is necessary
// - field arrays.
// - scene hierarchy -> update properties panel -> all of them will be table
// - scene settings
// - serialize runtime
// - add new project popup
// - all the project settings
// - fix font asset
// - fix content_browser_panel asset tree
// - symmetric content_browser_panel settings
// - fix animation editor drag drop size
// - there is an issue with scene_hierarchy_panel::draw_component (i guess...)
// - texture manager -> g_icons with this
// - audio_source destroy

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
	struct scene_history_entry;

	bool on_key_pressed(key_pressed_event& evnt);
	bool on_mouse_button_pressed(mouse_button_pressed_event& evnt);
	bool on_window_drop(window_drop_event& evnt);

	void draw_editor_grid();
	void on_overlay_render();

	bool new_project(const UI::project_create_settings& settings);
	void save_project();
	void finish_project_settings();

	bool open_project();
	bool open_project(const std::filesystem::path& path);
	bool has_project_loaded() const;
	void setup_project_loader();
	void load_recent_projects();
	void save_recent_projects() const;
	void add_recent_project(const std::filesystem::path& path);
	bool forget_recent_project(const std::filesystem::path& path);
	bool delete_recent_project(const std::filesystem::path& path);
	bool should_include_recent_project(const std::filesystem::path& path) const;
	std::filesystem::path get_recent_projects_path() const;
	std::filesystem::path get_preferences_path() const;
	void load_editor_preferences();
	void save_editor_preferences() const;
	void apply_preferences_to_content_browser();

	void new_scene();
	void open_scene(asset_handle handle);
	void close_scene();
	void save_scene();
	void save_scene_as();
	void save_entity_template(entity entity_in);
	bool instantiate_entity_template(asset_handle handle);
	bool handle_viewport_asset_drop(asset_handle handle);
	bool create_sprite_entity_from_texture(asset_handle handle, const glm::vec3& position);
	asset_handle import_external_asset_file(const std::filesystem::path& source_path);
	glm::vec3 get_viewport_mouse_world_position() const;

	bool build_project_scripts();
	void reload_assembly(bool reset_app_assembly_filepath = true);
	void start_script_source_watcher();
	void stop_script_source_watcher();
	void handle_script_source_event(const std::string& path, filewatch::Event event_type);
	void process_script_source_changes();
	void set_script_build_status(const std::string& message, bool warning = false, bool failure = false);
	void process_runtime_scene_transition();
	bool load_runtime_scene(asset_handle handle);
	bool unload_runtime_scene();
	void stop_active_runtime_scene_for_transition();
	void start_active_runtime_scene_for_transition(asset_handle handle);

	void serialize_scene(ref<scene> scene_in, const std::filesystem::path& path);

	void on_scene_play();
	void on_scene_simulate();
	void on_scene_stop();
	void on_scene_pause();

	void on_duplicated_entity();
	void on_deleted_entity();
	void on_select_all_entities();
	void on_copy_entities();
	void on_paste_entities();
	void on_cut_entities();
	void undo_scene();
	void redo_scene();
	void capture_scene_history(bool include_project_snapshot = false);
	void restore_scene_history(const scene_history_entry& entry);
	void clear_scene_history();
	struct project_history_entry;
	project_history_entry capture_project_history() const;
	void restore_project_history(const project_history_entry& entry);
	bool execute_editor_action(UI::editor_shortcut_action action);
	bool is_editor_action_available(UI::editor_shortcut_action action) const;
	void open_command_palette();
	void draw_command_palette();

	void UI_toolbar();
private:
	enum class scene_state
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
	UI::UI_project_loader m_project_loader;
	UI::UI_project m_UI_project;
	UI::UI_settings m_UI_settings;
	UI::UI_statistics m_UI_statistics;
	UI::popup_handler m_popup_handler;
	std::vector<std::filesystem::path> m_recent_projects;
	std::filesystem::path m_last_project_path;
	content_browser_panel::preferences m_content_browser_preferences;
	bool m_has_content_browser_preferences = false;
	bool m_command_palette_open = false;
	bool m_command_palette_focus_search = false;
	char m_command_palette_filter[128]{ 0 };

	scope<filewatch::FileWatch<std::string>> m_script_source_watcher;
	std::filesystem::path m_script_source_watch_directory;
	std::mutex m_script_source_mutex;
	std::chrono::steady_clock::time_point m_last_script_source_change_time{};
	std::filesystem::path m_last_script_source_change_path;
	std::string m_last_script_source_change_event;
	bool m_script_source_dirty = false;
	bool m_script_source_queued_while_running = false;
	std::string m_script_build_status = "Scripts idle";
	std::chrono::steady_clock::time_point m_script_build_status_time{};
	bool m_script_build_status_warning = false;
	bool m_script_build_status_failure = false;

	struct project_history_entry
	{
		bool valid = false;
		project_config config;
		std::filesystem::path project_path;
		std::filesystem::path asset_registry_path;
		std::string project_file_contents;
		std::string asset_registry_contents;
		std::unordered_map<std::string, std::string> scene_file_contents;
	};

	struct scene_history_entry
	{
		ref<scene> scene_snapshot;
		std::filesystem::path editor_scene_path;
		std::vector<UUID> selected_entities;
		project_history_entry project_snapshot;
	};

	// scene
	ref<scene> m_active_scene;
	ref<scene> m_editor_scene;
	std::filesystem::path m_editor_scene_path;
	std::vector<scene_history_entry> m_undo_stack;
	std::vector<scene_history_entry> m_redo_stack;
	std::vector<UUID> m_entity_clipboard;
	bool m_gizmo_history_active = false;

	// framebuffer
	ref<framebuffer> m_framebuffer;

	// gizmo
	int m_gizmo_type = -1;
	bool m_gizmo_hovered = false;
	bool m_gizmo_using = false;

	// states
	scene_state m_scene_state = scene_state::edit;

	// panels
	scene_hierarchy_panel m_scene_hierarchy_panel;
	animation_editor_panel m_animation_editor_panel;
	scope<content_browser_panel> m_content_browser_panel;

	ref<audio_source> m_audio_src;
};

_WHIP_END
