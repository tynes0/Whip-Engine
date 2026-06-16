#include "WhipPch.h"
#include <Whip/UI/UIStatistics.h>

#include <Whip/Render/Renderer2D.h>

#include <imgui.h>

_WHIP_START

namespace UI
{
	void UIStatistics::AddImage(ImTextureID textureId, const ImVec2& size, const ImVec2& uv1, const ImVec2& uv2)
	{
		m_ImageData.push_back({ textureId, size, uv1, uv2 });
	}

	void UIStatistics::SetOpen(bool open)
	{
		if (m_Open == open)
			return;
		m_Open = open;
		m_OpenDirty = true;
	}

	bool UIStatistics::ConsumeOpenDirty()
	{
		const bool dirty = m_OpenDirty;
		m_OpenDirty = false;
		return dirty;
	}

	void UI::UIStatistics::OnImGuiRender(Timestep ts)
	{
		if (!m_Open)
			return;

		static int count = 100;
		static float fps = 1000.0f / ts.GetMilliseconds();
		auto stats = Renderer2D::GetStats();
		bool open = m_Open;
		ImGui::Begin("Statistics", &open);
		if (open != m_Open)
			SetOpen(open);
		ImGui::Text("Draw Calls: %d", stats.m_DrawCalls);
		ImGui::Text("Quads: %d", stats.m_QuadCounts);
		ImGui::Text("Vertices: %d", stats.GetTotalVertexCount());
		ImGui::Text("Indices: %d", stats.GetTotalIndexCount());
		ImGui::Text("Fps: %f", fps);
		if (count-- == 0)
		{
			fps = 1000.0f / ts.GetMilliseconds();
			count = 100;
		}
		
		ImGui::End();
	}
}

_WHIP_END
