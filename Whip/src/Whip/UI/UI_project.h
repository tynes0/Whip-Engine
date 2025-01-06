#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/memory.h>
#include <Whip/Project/project.h>

#include "UI_popup_handler.h"

#include <string>

_WHIP_START

namespace UI
{
	class UI_project
	{
	public:
		using callback_type = std::function<void()>;

		enum UI_type { UI_none = 0, UI_settings };

		static constexpr size_t max_buffer_size = 128;

		UI_project();

		void set_finish_callback(const callback_type& callback);

		void show(UI_type type, const callback_type& callback = callback_type{});

		void on_imgui_render();
	private:
		UI_type m_type = UI_type::UI_none;
		UI_type m_temporary_type = UI_type::UI_none;

		callback_type m_callback;

		char m_name_buffer[max_buffer_size]{ 0 };
		char m_project_path_buffer[max_buffer_size]{ 0 };
		char m_asset_dir_buffer[max_buffer_size]{ 0 };
		char m_start_scene_buffer[max_buffer_size]{ 0 };
		char m_script_module_path_buffer[max_buffer_size]{ 0 };

		ref<project> m_last_active = nullptr;
	};
}

_WHIP_END
