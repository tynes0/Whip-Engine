#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/memory.h>
#include <Whip/Project/project.h>

#include "UI_popup_handler.h"

#include <filesystem>
#include <functional>
#include <string>

_WHIP_START

namespace UI
{
	class UI_project
	{
	public:
		using callback_type = std::function<void()>;
		using scene_callback_type = std::function<void(asset_handle)>;
		using scene_path_callback_type = std::function<std::filesystem::path()>;

		enum UI_type { UI_none = 0, UI_settings };

		static constexpr size_t max_buffer_size = 128;

		UI_project();

		void set_finish_callback(const callback_type& callback);
		void set_before_change_callback(const callback_type& callback);
		void set_scene_callbacks(const scene_callback_type& open_scene_callback, const callback_type& close_scene_callback, const scene_path_callback_type& active_scene_path_callback);
		void set_editor_settings_drawer(const callback_type& drawer);

		void show(UI_type type, const callback_type& callback = callback_type{});

		void on_imgui_render();
	private:
		void sync_from_active_project();
		void draw_project_settings();
		void draw_scene_settings();
		void draw_editor_settings();
		void draw_create_scene_popup();
		void draw_delete_scene_popup();
		void notify_before_change();
		void apply_project_settings(bool notify_change = true);
		void create_scene_from_popup();
		void delete_pending_scene();

		enum class settings_tab
		{
			project = 0,
			scenes,
			editor
		};

		UI_type m_type = UI_type::UI_none;
		UI_type m_temporary_type = UI_type::UI_none;
		settings_tab m_active_settings_tab = settings_tab::project;

		callback_type m_callback;
		callback_type m_before_change_callback;
		callback_type m_editor_settings_drawer;
		scene_callback_type m_open_scene_callback;
		callback_type m_close_scene_callback;
		scene_path_callback_type m_active_scene_path_callback;

		char m_name_buffer[max_buffer_size]{ 0 };
		char m_project_path_buffer[max_buffer_size]{ 0 };
		char m_asset_dir_buffer[max_buffer_size]{ 0 };
		char m_start_scene_buffer[max_buffer_size]{ 0 };
		char m_script_module_path_buffer[max_buffer_size]{ 0 };
		char m_new_scene_name_buffer[max_buffer_size]{ 0 };

		asset_handle m_pending_delete_scene = 0;
		std::filesystem::path m_pending_delete_scene_path;

		ref<project> m_last_active = nullptr;
	};
}

_WHIP_END
