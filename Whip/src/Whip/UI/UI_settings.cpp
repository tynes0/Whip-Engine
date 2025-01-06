#include "whippch.h"
#include "UI_settings.h"

#include <imgui.h>
#include "UI_scoped_style.h"
#include "UI_helpers.h"

_WHIP_START

namespace UI
{
	void UI_settings::on_imgui_render()
	{
		if(m_open)
		{
			ImGui::Begin("Settings", &m_open);
			{
				UI::scoped_style_bold_font font_style;
				ImGui::Checkbox("Show physics colliders", &m_show_physics_colliders);
				ImGui::Columns(1);
				ImGui::InputInt("Step Frame", &m_step_frame);
				ImGui::Text("Snap values");
			}
			UI::draw_vec3_control("Translation", m_snap_values[0]);
			UI::draw_vec3_control("Rotation", m_snap_values[1]);
			UI::draw_vec3_control("Scale", m_snap_values[2]);
			ImGui::End();
		}
	}
}

_WHIP_END
