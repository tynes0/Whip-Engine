#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Animation/animation2D.h>

#include <vector>
#include <string>
#include <functional>

#include <glm/vec3.hpp>

_WHIP_START

namespace UI
{
	void drag_drop_target(asset_type type, const std::function<void(asset_handle)>& callback, const char* label, bool draw_button = true, float x_size = 0.0f, float y_size = 0.0f, bool visible = true, const std::function<void()>& error_callback = nullptr);

	void draw_vec3_control(const std::string& label, glm::vec3& values, float reset_value = 0.0f, float column_width = 100.0f, float spacing = 0.0f, bool no_text = false);
	bool draw_field_vec2_control(const std::string& label_for_id, glm::vec2& values, float reset_value = 0.0f, float column_width = 100.0f);
	bool draw_field_vec3_control(const std::string& label_for_id, glm::vec3& values, float reset_value = 0.0f, float column_width = 100.0f);

	void draw_dual_handle_slider(float slider_min, float slider_max, float* value1, float* value2, float slider_width = 0.0f, float slider_height = 0.0f, bool show_texts = true);
	void draw_timeline_with_nodes_sl(ref<animation2D> anim, float initial_draw_range = 10.0f, float timeline_width = 200.0f, float timeline_height = 100.0f, float total_height = 150.0f, float max_v = FLT_MAX, int* selected_index = nullptr);
}

_WHIP_END
