#pragma once

#include <Whip/Core/Core.h>

#include <imgui.h>

_WHIP_START

namespace UI
{
	struct scoped_style_color
	{
		scoped_style_color() = default;

		scoped_style_color(ImGuiCol idx, ImVec4 color, bool predicate = true): m_set(predicate)
		{
			if (predicate)
				ImGui::PushStyleColor(idx, color);
		}

		scoped_style_color(ImGuiCol idx, ImU32 color, bool predicate = true) : m_set(predicate)
		{
			if (predicate)
				ImGui::PushStyleColor(idx, color);
		}

		~scoped_style_color()
		{
			if (m_set)
				ImGui::PopStyleColor();
		}
	private:
		bool m_set = false;
	};

	struct scoped_style_bold_font
	{
		scoped_style_bold_font(bool predicate = true) : m_set(predicate)
		{
			if (predicate)
			{
				ImGuiIO& io = ImGui::GetIO();
				auto boldFont = io.Fonts->Fonts[0];
				ImGui::PushFont(boldFont);
			}
		}

		~scoped_style_bold_font()
		{
			if (m_set)
				ImGui::PopFont();
		}
	private:
		bool m_set = false;
	};
}

_WHIP_END
