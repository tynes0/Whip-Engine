#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/buffer.h>
#include <Whip/Asset/asset.h>

#include <functional>
#include <string>

_WHIP_START

namespace UI
{
	class popup_handler
	{

	public:
		using function_type = std::function<void()>;
		using function_type_with_data = std::function<void(raw_buffer)>;

		popup_handler() = default;
		popup_handler(const std::string& name) : m_popup_name(name) {}
	
		popup_handler& set_popup_name(const std::string& popup_name) { m_popup_name = popup_name; return *this; }
		popup_handler& set_size(float width, float height) { m_width = width; m_height = height; return *this; }
		popup_handler& set_width(float width) { m_width = width; return *this; }
		popup_handler& set_height(float height) { m_height = height; return *this; }
		popup_handler& set_show_state(bool state) { m_show_state = state; return *this; }

		popup_handler& add(const function_type& func);
		popup_handler& add(const function_type_with_data& func);
		popup_handler& add_button(const function_type& callback, const std::string& label, float width = 0.0f, float height = 0.0f, bool visible = true);
		popup_handler& add_button(const function_type_with_data& callback, const std::string& label, float width = 0.0f, float height = 0.0f, bool visible = true);
		popup_handler& add_drag_drop(asset_type type, const std::function<void(asset_handle)>& callback, const char* label, float width, float height, bool visible = true, const std::function<void()>& error_callback = nullptr);
		popup_handler& add_dual_handle_slider(float slider_min, float slider_max, float* value1, float* value2, float slider_width = 0.0f, float slider_height = 0.0f, bool show_texts = true);
		popup_handler& same_line();

		const std::string& get_popup_name() const { return m_popup_name; }
		bool get_show_state() const { return m_show_state; }
		float get_width() const { return m_width; }
		float get_height() const { return m_height; }
	
		void on_imgui_render();
		void load_user_data(raw_buffer user_data);

		static constexpr float default_width = 100.0f;
		static constexpr float default_height = 100.0f;
	private:
		std::string m_popup_name;
		std::vector<function_type> m_draw_list;
		std::vector<function_type> m_draw_list_with_data;
		float m_width = default_width, m_height = default_height;
		bool m_show_state = false;
		raw_buffer m_user_data = nullptr;
	};
}

_WHIP_END
