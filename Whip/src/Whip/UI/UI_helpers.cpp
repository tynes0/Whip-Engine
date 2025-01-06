#include "whippch.h"
#include "UI_helpers.h"

#include "UI_scoped_style.h"

#include "Whip/Core/Input.h"
#include "Whip/Asset/asset_manager.h"

#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>

#include <glm/glm.hpp>

_WHIP_START

namespace utils
{
	static void check_range(float range_min, float range_max, float* value1, float* value2)
	{
		if (*value1 > range_max) *value1 = range_max;
		else if (*value1 < range_min) *value1 = range_min;
		if (*value2 > range_max) *value2 = range_max;
		else if (*value2 < range_min) *value2 = range_min;
	}
}

namespace UI
{
	void drag_drop_target(asset_type type, const std::function<void(asset_handle)>& callback, const char* label, bool draw_button, float x_size, float y_size, bool visible, const std::function<void()>& error_callback)
	{
		if (type == asset_type::none)
		{
			WHP_CORE_ERROR("[Asset Manager] Undefined type!");
			return;
		}
		ImVec2 cursor_pos = ImGui::GetCursorPos();

		if(draw_button)
		{
			if (visible)
			{
				ImGui::Button(label, ImVec2(x_size, y_size));
			}
			else
			{
				ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
				ImGui::InvisibleButton(label, ImVec2(x_size, y_size), ImGuiButtonFlags_AllowOverlap);
				ImGui::PopItemFlag();
			}
		}

		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
			{
				asset_handle handle = *(asset_handle*)payload->Data;
				if (asset_manager::get_asset_type(handle) == type)
				{
					callback(handle);
				}
				else
				{
					if (error_callback)
						error_callback();
					else
						WHP_CORE_WARN("[Asset Manager] Wrong asset type!");
				}
			}
			ImGui::EndDragDropTarget();
		}
		if(!visible)
			ImGui::SetCursorPos(cursor_pos);
	}

	void draw_vec3_control(const std::string& label, glm::vec3& values, float reset_value, float column_width, float spacing, bool no_text)
	{
		ImGui::PushID(label.c_str());

		if (!no_text)
		{
			ImGui::Columns(2);
			ImGui::SetColumnWidth(0, column_width);
			ImGui::Text(label.c_str());
			ImGui::NextColumn();
		}

		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0, 0 });
		
		float line_height = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
		ImVec2 buttonSize = { line_height + 3.0f, line_height };
		ImGui::PushMultiItemsWidths(3, ImGui::GetWindowWidth() - column_width - buttonSize.x * 3 - spacing);

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 1.0f, 0.0f, 0.0f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.3f, 0.3f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.9f, 0.2f, 0.2f, 1.0f });
		{
			scoped_style_color scope_color(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
			scoped_style_bold_font bold_font;
			if (ImGui::Button("X", buttonSize))
				values.x = reset_value;
		}
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::DragFloat("##X", &values.x, 0.1f, 0.0f, 0.0f, "%.2f");
		ImGui::PopItemWidth();
		ImGui::SameLine();

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.0f, 0.8f, 0.0f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.2f, 0.6f, 0.2f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.1f, 0.8f, 0.1f, 1.0f });
		{
			scoped_style_color scope_color(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
			scoped_style_bold_font bold_font;
			if (ImGui::Button("Y", buttonSize))
				values.y = reset_value;
		}
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::DragFloat("##Y", &values.y, 0.1f, 0.0f, 0.0f, "%.2f");
		ImGui::PopItemWidth();
		ImGui::SameLine();

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.0f, 0.0f, 1.0f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.3f, 0.3f, 0.9f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.2f, 0.2f, 0.9f, 1.0f });
		{
			scoped_style_color scope_color(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
			scoped_style_bold_font bold_font;
			if (ImGui::Button("Z", buttonSize))
				values.z = reset_value;
		}
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::DragFloat("##Z", &values.z, 0.1f, 0.0f, 0.0f, "%.2f");
		ImGui::PopItemWidth();

		ImGui::PopStyleVar();

		ImGui::Columns(1);

		ImGui::PopID();
	}
	bool draw_field_vec2_control(const std::string& label_for_id, glm::vec2& values, float reset_value, float column_width)
	{
		ImGui::PushID(label_for_id.c_str());

		bool changed = false;

		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0, 0 });

		float line_height = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
		ImVec2 buttonSize = { line_height + 3.0f, line_height };

		column_width -= buttonSize.x * 2;

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 1.0f, 0.0f, 0.0f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.3f, 0.3f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.9f, 0.2f, 0.2f, 1.0f });
		{
			scoped_style_color scope_color(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
			scoped_style_bold_font bold_font;
			if (ImGui::Button("X", buttonSize))
			{
				values.x = reset_value;
				changed = true;
			}
		}
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::PushItemWidth(column_width / 2);
		if (ImGui::DragFloat("##X", &values.x, 0.1f, 0.0f, 0.0f, "%.2f"))
			changed = true;
		ImGui::PopItemWidth();
		ImGui::SameLine();

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.0f, 0.8f, 0.0f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.2f, 0.6f, 0.2f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.1f, 0.8f, 0.1f, 1.0f });
		{
			scoped_style_color scope_color(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
			scoped_style_bold_font bold_font;
			if (ImGui::Button("Y", buttonSize))
			{
				values.y = reset_value;
				changed = true;
			}
		}
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::PushItemWidth(-1);
		if (ImGui::DragFloat("##Y", &values.y, 0.1f, 0.0f, 0.0f, "%.2f"))
			changed = true;
		ImGui::PopItemWidth();

		ImGui::PopStyleVar();

		ImGui::PopID();
		return changed;
	}	

	bool draw_field_vec3_control(const std::string& label_for_id, glm::vec3& values, float reset_value, float column_width)
	{
		ImGui::PushID(label_for_id.c_str());

		bool changed = false;

		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0, 0 });

		float line_height = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
		ImVec2 button_size = { line_height + 3.0f, line_height };

		column_width -= button_size.x * 3;

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 1.0f, 0.0f, 0.0f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.3f, 0.3f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.9f, 0.2f, 0.2f, 1.0f });
		{
			scoped_style_color scope_color(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
			scoped_style_bold_font bold_font;
			if (ImGui::Button("X", button_size))
			{
				values.x = reset_value;
				changed = true;
			}
		}
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::PushItemWidth(column_width / 3);
		if(ImGui::DragFloat("##X", &values.x, 0.1f, 0.0f, 0.0f, "%.2f"))
			changed = true;
		ImGui::PopItemWidth();
		ImGui::SameLine();

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.0f, 0.8f, 0.0f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.2f, 0.6f, 0.2f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.1f, 0.8f, 0.1f, 1.0f });
		{
			scoped_style_color scope_color(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
			scoped_style_bold_font bold_font;
			if (ImGui::Button("Y", button_size))
			{
				values.y = reset_value;
				changed = true;
			}
		}
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::PushItemWidth(column_width / 3);
		if(ImGui::DragFloat("##Y", &values.y, 0.1f, 0.0f, 0.0f, "%.2f"))
			changed = true;
		ImGui::PopItemWidth();
		ImGui::SameLine();

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.0f, 0.0f, 1.0f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.3f, 0.3f, 0.9f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.2f, 0.2f, 0.9f, 1.0f });
		{
			scoped_style_color scope_color(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
			scoped_style_bold_font bold_font;
			if (ImGui::Button("Z", button_size))
			{
				values.z = reset_value;
				changed = true;
			}
		}
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::PushItemWidth(-1);
		if(ImGui::DragFloat("##Z", &values.z, 0.1f, 0.0f, 0.0f, "%.2f"))
			changed = true;
		ImGui::PopItemWidth();

		ImGui::PopStyleVar();

		ImGui::PopID();
		return changed;
	}

	void draw_dual_handle_slider(float slider_min, float slider_max, float* value1, float* value2, float slider_width, float slider_height, bool show_texts)
	{
		utils::check_range(slider_min, slider_max, value1, value2);

		ImVec2 slider_pos = ImGui::GetCursorScreenPos();
		if(slider_width == 0.0f)
			slider_width = ImGui::GetContentRegionAvail().x;
		if(slider_height == 0.0f)
			slider_height = 20.0f;

		ImDrawList* draw_list = ImGui::GetWindowDrawList();

		ImU32 bg_color = IM_COL32(50, 50, 50, 255);
		ImU32 fill_color = IM_COL32(102, 40, 40, 255);
		ImU32 handle_color = IM_COL32(160, 160, 160, 196);
		float range_start_x = slider_pos.x + (*value1 - slider_min) / (slider_max - slider_min) * slider_width;
		float range_end_x = slider_pos.x + (*value2 - slider_min) / (slider_max - slider_min) * slider_width;

		draw_list->AddRectFilled(slider_pos, ImVec2(slider_pos.x + slider_width, slider_pos.y + slider_height), bg_color, 1.0f);
		draw_list->AddRectFilled(ImVec2(range_start_x, slider_pos.y), ImVec2(range_end_x, slider_pos.y + slider_height), fill_color, 1.0f);
		draw_list->AddRectFilled(ImVec2(range_start_x - 4, slider_pos.y), ImVec2(range_start_x + 4, slider_pos.y + slider_height), handle_color, 2.0f);
		draw_list->AddRectFilled(ImVec2(range_end_x - 4, slider_pos.y), ImVec2(range_end_x + 4, slider_pos.y + slider_height), handle_color, 2.0f);

		ImGui::InvisibleButton("##slider", ImVec2(slider_width, slider_height));
		if (ImGui::IsItemActive())
		{
			ImVec2 mouse_pos = ImGui::GetIO().MousePos;
			float normalized_pos = (mouse_pos.x - slider_pos.x) / slider_width;
			float value = slider_min + normalized_pos * (slider_max - slider_min);

			if (ImGui::IsMouseDown(0))
			{
				if (fabsf(value - *value1) < fabsf(value - *value2))
					*value1 = ImClamp(value, slider_min, *value2);
				else
					*value2 = ImClamp(value, *value1, slider_max);
			}
		}

		if(show_texts)
			ImGui::Text("Selected Range: %.2f - %.2f", *value1, *value2);
	}

	void draw_timeline_with_nodes_sl(ref<animation2D> anim, float initial_draw_range, float timeline_width, float timeline_height, float total_height, float max_v, int* selected_index)
	{
		static constexpr float max_initial_range = 20.0f;
		static constexpr float zoom_min = 0.25f;
		static constexpr float zoom_max = 20.0f;
		static constexpr float zoom_speed = 0.1f;
		static constexpr float major_interval = 1.0f;
		static constexpr float minor_interval = 0.1f;
		static constexpr float mini_interval = 0.01f;

		static constexpr ImU32 major_line_color = IM_COL32(255, 255, 255, 255);
		static constexpr ImU32 minor_line_color = IM_COL32(150, 150, 150, 255);
		static constexpr ImU32 mini_line_color = IM_COL32(150, 150, 150, 255);
		static constexpr ImU32 seconds_text_color = IM_COL32(255, 255, 255, 255);
		static constexpr ImU32 window_bg_color = IM_COL32(50, 50, 50, 255);
		static constexpr ImU32 timeline_bg_color = IM_COL32(100, 100, 100, 200);
		static constexpr ImU32 node_color = IM_COL32(192, 40, 40, 255);

		static constexpr ImVec2 node_size(12, 12);
		static constexpr float node_radius = node_size.x / 2.0f;

		static float zoom_level = 1.0f;
		static float offset_time = 0.0f;

		float zoom_threshold = initial_draw_range * 0.6f;


		static constexpr auto draw_major_line = [](ImDrawList* draw_list, float x, float cursor_y, float timeline_height, float time)
			{
				draw_list->AddLine(ImVec2(x, cursor_y), ImVec2(x, cursor_y + timeline_height), major_line_color);
				char label[16];
				snprintf(label, sizeof(label), "%.0f", time);
				draw_list->AddText(ImVec2(x + 2, cursor_y + timeline_height + 4), seconds_text_color, label);
			};

		static constexpr auto draw_minor_line = [](ImDrawList* draw_list, float x, float cursor_y, float timeline_height)
			{
				draw_list->AddLine(ImVec2(x, cursor_y), ImVec2(x, cursor_y + timeline_height * 0.4f), minor_line_color);
			};

		static constexpr auto draw_mini_line = [](ImDrawList* draw_list, float x, float cursor_y, float timeline_height)
			{
				draw_list->AddLine(ImVec2(x, cursor_y), ImVec2(x, cursor_y + timeline_height * 0.2f), mini_line_color);
			};

		static const auto drag_drop_callback = [anim](asset_handle handle)
			{
				animation_frame new_frame = {};
				new_frame.texture = handle;
				anim->add_frame(new_frame);
			};

		if (timeline_width == 0.0f)
			timeline_width = ImGui::GetContentRegionAvail().x;
		if (timeline_height == 0.0f)
			timeline_height = 100.0f;
		if (total_height == 0.0f)
			total_height = timeline_height + 50.0f;

		if (initial_draw_range > max_initial_range)
			initial_draw_range = max_initial_range;

		ImGui::BeginChild("TimelineRegion", ImVec2(0, total_height), true);

		// drag - drop
		{
			ImVec2 window_size = ImGui::GetWindowSize();
			ImGuiStyle style = ImGui::GetStyle();

			window_size.x -= style.WindowPadding.x + style.ItemSpacing.x * 2;
			window_size.y -= style.WindowPadding.y + style.ItemSpacing.y * 2;

			drag_drop_target(asset_type::texture2D, drag_drop_callback, "DragDropTexture", true, window_size.x, window_size.y, false);
		}

		ImDrawList* draw_list = ImGui::GetWindowDrawList();
		ImVec2 cursor_start = ImGui::GetCursorScreenPos();

		// background colors
		ImVec2 child_min = ImGui::GetWindowPos();
		draw_list->AddRectFilled(child_min, ImVec2(child_min.x + ImGui::GetWindowWidth(), child_min.y + ImGui::GetWindowHeight()), window_bg_color, ImGui::GetStyle().WindowRounding);
		draw_list->AddRectFilled(cursor_start, ImVec2(cursor_start.x + timeline_width, cursor_start.y + timeline_height), timeline_bg_color);

		// timeline moves
		if (ImGui::IsWindowHovered() && input::is_key_down(key::left_control))
		{
			float scroll_delta = input::get_scroll_delta();
			if (scroll_delta != 0.0f)
			{
				float mouse_x = input::get_mouse_X() - cursor_start.x;
				float zoom_center = offset_time + (mouse_x / timeline_width) * (initial_draw_range / zoom_level);

				float new_zoom_level = std::clamp(zoom_level * (1.0f + scroll_delta * zoom_speed), zoom_min, zoom_max);
				offset_time = std::clamp(zoom_center - (zoom_center - offset_time) * (zoom_level / new_zoom_level), 0.0f, initial_draw_range);
				zoom_level = new_zoom_level;
			}
			if (ImGui::IsMouseDragging(0))
			{
				float delta_x = ImGui::GetMouseDragDelta().x;
				ImGui::ResetMouseDragDelta();

				float delta_time = (delta_x / timeline_width) * (initial_draw_range / zoom_level);
				offset_time = std::max(0.0f, offset_time - delta_time);
			}
		}

		float scaled_max_time = initial_draw_range / zoom_level;
		float start_time = std::floor(offset_time / minor_interval) * minor_interval;
		float end_time = std::ceil((offset_time + scaled_max_time) / minor_interval) * minor_interval;
		float interval = zoom_level < zoom_threshold ? minor_interval : mini_interval;

		for (float time = start_time; time <= end_time; time += interval)
		{
			float x = cursor_start.x + ((time - offset_time) / scaled_max_time) * timeline_width;

			if (x < cursor_start.x || x > cursor_start.x + timeline_width)
				continue;

			if (zoom_level < zoom_threshold)
			{
				if (static_cast<int>(std::round(time * 10.0f)) % static_cast<int>(std::round(major_interval * 10.0f)) == 0)
					draw_major_line(draw_list, x, cursor_start.y, timeline_height, time);
				else
					draw_minor_line(draw_list, x, cursor_start.y, timeline_height);
			}
			else
			{
				if (static_cast<int>(std::round(time * 100.0f)) % static_cast<int>(std::round(major_interval * 100.0f)) == 0)
					draw_major_line(draw_list, x, cursor_start.y, timeline_height, time);
				else if(static_cast<int>(std::round(time * 100.0f)) % static_cast<int>(std::round(minor_interval * 100.0f)) == 0)
					draw_minor_line(draw_list, x, cursor_start.y, timeline_height);
				else
					draw_mini_line(draw_list, x, cursor_start.y, timeline_height);
			}
		}

		// node renders
		if (anim)
		{
			auto& frames = anim->get_frames();

			for (size_t i = 0; i < frames.size(); ++i)
			{
				float scaled_time = (frames[i].duration - offset_time) / scaled_max_time;

				if (scaled_time < 0.0f || scaled_time > 1.0f)
					continue;

				float x_pos = cursor_start.x + scaled_time * timeline_width;
				ImVec2 node_pos = ImVec2(x_pos, cursor_start.y + timeline_height / 2);

				draw_list->AddCircleFilled(node_pos, node_radius, node_color);
				ImGui::SetCursorScreenPos(ImVec2(node_pos.x - node_radius, node_pos.y - node_radius));
				ImGui::InvisibleButton(("node_drag" + std::to_string(i)).c_str(), node_size);

				if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0))
				{
					float delta_x = ImGui::GetMouseDragDelta().x;
					ImGui::ResetMouseDragDelta();

					float delta_time = (delta_x / timeline_width) * scaled_max_time;

					frames[i].duration = std::clamp(frames[i].duration + delta_time, 0.0f, max_v);
					ImGui::SetTooltip("Frame %zu: %.3fs", i, frames[i].duration);
				}
				else
				{
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("Frame %zu: %.3fs", i, frames[i].duration);
						if (input::is_mouse_button_down(mouse::button0))
							if (selected_index)
								*selected_index = static_cast<int>(i);
					}
				}
			}
		}
		ImGui::EndChild();
	}
}

_WHIP_END
