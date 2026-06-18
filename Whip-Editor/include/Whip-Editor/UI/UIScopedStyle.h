#pragma once

#include <Whip/Core/Core.h>

#include <imgui.h>

_WHIP_START

namespace UI
{
	struct ScopedStyleColor
	{
		ScopedStyleColor() = default;

		ScopedStyleColor(ImGuiCol idx, ImVec4 color, bool predicate = true): m_Set(predicate)
		{
			if (predicate)
				ImGui::PushStyleColor(idx, color);
		}

		ScopedStyleColor(ImGuiCol idx, ImU32 color, bool predicate = true) : m_Set(predicate)
		{
			if (predicate)
				ImGui::PushStyleColor(idx, color);
		}

		~ScopedStyleColor()
		{
			if (m_Set)
				ImGui::PopStyleColor();
		}
	private:
		bool m_Set = false;
	};

	struct ScopedStyleBoldFont
	{
		ScopedStyleBoldFont(bool predicate = true) : m_Set(predicate)
		{
			if (predicate)
			{
				ImGuiIO& io = ImGui::GetIO();
				auto boldFont = io.Fonts->Fonts[0];
				ImGui::PushFont(boldFont);
			}
		}

		~ScopedStyleBoldFont()
		{
			if (m_Set)
				ImGui::PopFont();
		}
	private:
		bool m_Set = false;
	};
}

_WHIP_END
