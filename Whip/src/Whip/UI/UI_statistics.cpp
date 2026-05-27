#include "whippch.h"
#include "UI_statistics.h"

#include <Whip/Render/Renderer2D.h>

#include <imgui.h>

_WHIP_START

namespace UI
{
	void UI_statistics::add_image(ImTextureID texture_id, const ImVec2& size, const ImVec2& uv1, const ImVec2& uv2)
	{
		m_image_data.push_back({ texture_id, size, uv1, uv2 });
	}

	void UI_statistics::set_open(bool open)
	{
		if (m_open == open)
			return;
		m_open = open;
		m_open_dirty = true;
	}

	bool UI_statistics::consume_open_dirty()
	{
		const bool dirty = m_open_dirty;
		m_open_dirty = false;
		return dirty;
	}

	void UI::UI_statistics::on_imgui_render(timestep ts)
	{
		if (!m_open)
			return;

		static int count = 100;
		static float fps = 1000.0f / ts.get_milliseconds();
		auto stats = renderer2D::get_stats();
		bool open = m_open;
		ImGui::Begin("Statistics", &open);
		if (open != m_open)
			set_open(open);
		ImGui::Text("Draw Calls: %d", stats.draw_calls);
		ImGui::Text("Quads: %d", stats.quad_counts);
		ImGui::Text("Vertices: %d", stats.get_total_vertex_count());
		ImGui::Text("Indices: %d", stats.get_total_index_count());
		ImGui::Text("Fps: %f", fps);
		if (count-- == 0)
		{
			fps = 1000.0f / ts.get_milliseconds();
			count = 100;
		}
		
		ImGui::End();
	}
}

_WHIP_END
