#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Animation/animation2D.h>

#include <vector>
#include <string>
#include <functional>
#include <cstdint>
#include <type_traits>

#include <glm/vec3.hpp>
#include <imgui.h>

_WHIP_START

namespace UI
{

	WHP_NODISCARD inline ImTextureID to_imgui_texture_id(uint64_t renderer_id)
	{
		return renderer_id;
	}

	WHP_NODISCARD inline ImTextureID to_imgui_texture_id(uint32_t renderer_id)
	{
		return to_imgui_texture_id(static_cast<uint64_t>(renderer_id));
	}

	inline void image(ImTextureID texture_id, const ImVec2& size, const ImVec2& uv0 = ImVec2(0, 0), const ImVec2& uv1 = ImVec2(1, 1), const ImVec4& tint_col = ImVec4(1, 1, 1, 1), const ImVec4& border_col = ImVec4(0, 0, 0, 0))
	{
		ImGui::Image(texture_id, size, uv0, uv1, tint_col, border_col);
	}

	WHP_NODISCARD inline bool image_button(const char* id, ImTextureID texture_id, const ImVec2& size, const ImVec2& uv0 = ImVec2(0, 0), const ImVec2& uv1 = ImVec2(1, 1), int frame_padding = -1, const ImVec4& bg_col = ImVec4(0, 0, 0, 0), const ImVec4& tint_col = ImVec4(1, 1, 1, 1))
	{
#if defined(IMGUI_VERSION_NUM) && IMGUI_VERSION_NUM >= 18900
		WHP_UNUSED(frame_padding);
		return ImGui::ImageButton(id, texture_id, size, uv0, uv1, bg_col, tint_col);
#else
		WHP_UNUSED(id);
		return ImGui::ImageButton(texture_id, size, uv0, uv1, frame_padding, bg_col, tint_col);
#endif
	}

	void drag_drop_target(asset_type type, const std::function<void(asset_handle)>& callback, const char* label, bool draw_button = true, float x_size = 0.0f, float y_size = 0.0f, bool visible = true, const std::function<void()>& error_callback = nullptr);

	void draw_vec3_control(const std::string& label, glm::vec3& values, float reset_value = 0.0f, float column_width = 100.0f, float spacing = 0.0f, bool no_text = false);
	bool draw_field_vec2_control(const std::string& label_for_id, glm::vec2& values, float reset_value = 0.0f, float column_width = 100.0f);
	bool draw_field_vec3_control(const std::string& label_for_id, glm::vec3& values, float reset_value = 0.0f, float column_width = 100.0f);

	void draw_dual_handle_slider(float slider_min, float slider_max, float* value1, float* value2, float slider_width = 0.0f, float slider_height = 0.0f, bool show_texts = true);
	void draw_timeline_with_nodes_sl(ref<animation2D> anim, float initial_draw_range = 10.0f, float timeline_width = 200.0f, float timeline_height = 100.0f, float total_height = 150.0f, float max_v = FLT_MAX, int* selected_index = nullptr);
}

_WHIP_END
