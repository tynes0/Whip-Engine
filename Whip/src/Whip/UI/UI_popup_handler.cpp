#include <whippch.h>
#include "UI_popup_handler.h"

#include "UI_helpers.h"
#include <Whip/Core/Application.h>

#include <imgui.h>

_WHIP_START

UI::popup_handler& UI::popup_handler::add(const function_type& func)
{
	m_draw_list.emplace_back(func);
	return *this;
}

UI::popup_handler& UI::popup_handler::same_line()
{
	m_draw_list.emplace_back([]() { ImGui::SameLine(); });
	return *this;
}

UI::popup_handler& UI::popup_handler::add_button(const function_type& callback, const std::string& label, float width, float height, bool visible)
{
	m_draw_list.emplace_back([=]() 
		{  
			if (visible)
			{
				if (ImGui::Button(label.c_str(), ImVec2(width, height)))
					callback();
			}
			else
			{
				if (ImGui::InvisibleButton(label.c_str(), ImVec2(width, height)))
					callback();
			}
		});
	return *this;
}

UI::popup_handler& UI::popup_handler::add_drag_drop(asset_type type, const std::function<void(asset_handle)>& callback, const char* label, float width, float height, bool visible, const std::function<void()>& error_callback)
{
	m_draw_list.emplace_back([=]() 
		{
			UI::drag_drop_target(type, callback, label, true, width, height, visible, error_callback);
		});
	return *this;
}

UI::popup_handler& UI::popup_handler::add_dual_handle_slider(float slider_min, float slider_max, float* value1, float* value2, float slider_width, float slider_height, bool show_texts)
{
	m_draw_list.emplace_back([=]()
		{
			UI::draw_dual_handle_slider(slider_min, slider_max, value1, value2, slider_width, slider_height, show_texts);
		});
	return *this;
}

void UI::popup_handler::on_imgui_render()
{
	if (m_show_state && !m_popup_name.empty())
	{
		auto& app = application::get();
		auto& window = app.get_window();

		ImVec2 window_size{ (float)window.get_width(), (float)window.get_height() };
		ImVec2 window_pos{ (float)window.get_position().first, (float)window.get_position().second };
		ImVec2 popup_pos = ImVec2{ ((window_size.x - m_width) * 0.5f) + window_pos.x, ((window_size.y - m_height) * 0.5f) + window_pos.y };

		ImGui::SetNextWindowSize(ImVec2(m_width, m_height));
		ImGui::SetNextWindowPos(popup_pos);
		ImGui::OpenPopup(m_popup_name.c_str());

		if (ImGui::BeginPopupModal(m_popup_name.c_str(), NULL, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::PushTextWrapPos(m_width);
			for (const auto& func : m_draw_list)
				func();

			if (m_show_state && ImGui::IsMouseClicked(0) && !ImGui::IsAnyItemHovered() && !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow)) 
			{
				m_show_state = false;
				ImGui::CloseCurrentPopup();
			}
			ImGui::PopTextWrapPos();
			ImGui::EndPopup();
		}

	}
}

_WHIP_END
