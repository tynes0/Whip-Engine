#include "whippch.h"
#include "UI_settings.h"

#include <imgui.h>
#include "UI_helpers.h"

_WHIP_START

namespace UI
{
	namespace
	{
		struct theme_palette
		{
			ImVec4 text;
			ImVec4 text_disabled;
			ImVec4 window;
			ImVec4 child;
			ImVec4 popup;
			ImVec4 border;
			ImVec4 frame;
			ImVec4 frame_hovered;
			ImVec4 frame_active;
			ImVec4 title;
			ImVec4 title_active;
			ImVec4 menu;
			ImVec4 accent;
			ImVec4 accent_hovered;
			ImVec4 accent_active;
			ImVec4 tab;
			ImVec4 tab_hovered;
			ImVec4 tab_active;
			ImVec4 table_header;
			ImVec4 warning;
		};

		constexpr key_code editable_keys[] =
		{
			key::Q, key::W, key::E, key::R, key::T, key::Y, key::A, key::S, key::D, key::F, key::G, key::Z, key::X, key::C, key::V, key::B, key::N, key::O, key::P,
			key::D0, key::D1, key::D2, key::D3, key::D4, key::D5, key::D6, key::D7, key::D8, key::D9,
			key::F1, key::F2, key::F3, key::F4, key::F5, key::F6, key::F7, key::F8, key::F9, key::F10, key::F11, key::F12,
			key::escape, key::del, key::backspace, key::insert, key::space, key::enter, key::tab,
			key::left, key::right, key::up, key::down, key::home, key::end,
			key::comma, key::period, key::minus, key::equal, key::left_bracket, key::right_bracket, key::grave_accent
		};

		bool same_binding(const shortcut_binding& left, const shortcut_binding& right)
		{
			return left.key != 0 &&
				left.key == right.key &&
				left.ctrl == right.ctrl &&
				left.shift == right.shift &&
				left.alt == right.alt;
		}

		theme_palette get_dark_palette(editor_theme theme)
		{
			switch (theme)
			{
			case editor_theme::graphite:
				return {
					ImVec4(0.89f, 0.90f, 0.88f, 1.0f),
					ImVec4(0.52f, 0.53f, 0.51f, 1.0f),
					ImVec4(0.055f, 0.055f, 0.052f, 0.98f),
					ImVec4(0.075f, 0.075f, 0.071f, 1.0f),
					ImVec4(0.070f, 0.070f, 0.066f, 0.99f),
					ImVec4(0.185f, 0.180f, 0.165f, 1.0f),
					ImVec4(0.125f, 0.123f, 0.116f, 1.0f),
					ImVec4(0.180f, 0.176f, 0.164f, 1.0f),
					ImVec4(0.250f, 0.235f, 0.190f, 1.0f),
					ImVec4(0.047f, 0.047f, 0.045f, 1.0f),
					ImVec4(0.075f, 0.074f, 0.070f, 1.0f),
					ImVec4(0.065f, 0.064f, 0.060f, 1.0f),
					ImVec4(0.650f, 0.610f, 0.470f, 1.0f),
					ImVec4(0.750f, 0.690f, 0.520f, 1.0f),
					ImVec4(0.860f, 0.750f, 0.480f, 1.0f),
					ImVec4(0.080f, 0.079f, 0.074f, 1.0f),
					ImVec4(0.150f, 0.145f, 0.130f, 1.0f),
					ImVec4(0.125f, 0.118f, 0.100f, 1.0f),
					ImVec4(0.095f, 0.093f, 0.087f, 1.0f),
					ImVec4(0.880f, 0.560f, 0.300f, 1.0f)
				};
			case editor_theme::ember:
				return {
					ImVec4(0.92f, 0.89f, 0.84f, 1.0f),
					ImVec4(0.56f, 0.51f, 0.46f, 1.0f),
					ImVec4(0.070f, 0.055f, 0.045f, 0.98f),
					ImVec4(0.095f, 0.075f, 0.060f, 1.0f),
					ImVec4(0.090f, 0.070f, 0.056f, 0.99f),
					ImVec4(0.235f, 0.185f, 0.145f, 1.0f),
					ImVec4(0.155f, 0.120f, 0.095f, 1.0f),
					ImVec4(0.220f, 0.165f, 0.125f, 1.0f),
					ImVec4(0.330f, 0.185f, 0.115f, 1.0f),
					ImVec4(0.055f, 0.042f, 0.035f, 1.0f),
					ImVec4(0.095f, 0.070f, 0.054f, 1.0f),
					ImVec4(0.075f, 0.056f, 0.045f, 1.0f),
					ImVec4(0.900f, 0.520f, 0.260f, 1.0f),
					ImVec4(0.980f, 0.610f, 0.330f, 1.0f),
					ImVec4(1.000f, 0.720f, 0.420f, 1.0f),
					ImVec4(0.095f, 0.070f, 0.055f, 1.0f),
					ImVec4(0.190f, 0.125f, 0.085f, 1.0f),
					ImVec4(0.165f, 0.095f, 0.064f, 1.0f),
					ImVec4(0.115f, 0.085f, 0.065f, 1.0f),
					ImVec4(0.940f, 0.700f, 0.360f, 1.0f)
				};
			case editor_theme::moss:
				return {
					ImVec4(0.88f, 0.90f, 0.84f, 1.0f),
					ImVec4(0.51f, 0.54f, 0.47f, 1.0f),
					ImVec4(0.045f, 0.055f, 0.047f, 0.98f),
					ImVec4(0.060f, 0.075f, 0.064f, 1.0f),
					ImVec4(0.058f, 0.070f, 0.060f, 0.99f),
					ImVec4(0.150f, 0.185f, 0.145f, 1.0f),
					ImVec4(0.100f, 0.122f, 0.104f, 1.0f),
					ImVec4(0.140f, 0.170f, 0.142f, 1.0f),
					ImVec4(0.190f, 0.250f, 0.165f, 1.0f),
					ImVec4(0.040f, 0.050f, 0.042f, 1.0f),
					ImVec4(0.060f, 0.075f, 0.064f, 1.0f),
					ImVec4(0.050f, 0.062f, 0.054f, 1.0f),
					ImVec4(0.560f, 0.680f, 0.420f, 1.0f),
					ImVec4(0.650f, 0.760f, 0.500f, 1.0f),
					ImVec4(0.780f, 0.840f, 0.540f, 1.0f),
					ImVec4(0.065f, 0.080f, 0.068f, 1.0f),
					ImVec4(0.110f, 0.145f, 0.108f, 1.0f),
					ImVec4(0.105f, 0.130f, 0.090f, 1.0f),
					ImVec4(0.073f, 0.090f, 0.074f, 1.0f),
					ImVec4(0.820f, 0.620f, 0.300f, 1.0f)
				};
			case editor_theme::whip_dark:
			default:
				return {
					ImVec4(0.91f, 0.90f, 0.86f, 1.0f),
					ImVec4(0.55f, 0.54f, 0.50f, 1.0f),
					ImVec4(0.052f, 0.050f, 0.046f, 0.98f),
					ImVec4(0.070f, 0.067f, 0.060f, 1.0f),
					ImVec4(0.067f, 0.064f, 0.058f, 0.99f),
					ImVec4(0.190f, 0.175f, 0.145f, 1.0f),
					ImVec4(0.115f, 0.108f, 0.096f, 1.0f),
					ImVec4(0.170f, 0.154f, 0.130f, 1.0f),
					ImVec4(0.300f, 0.205f, 0.110f, 1.0f),
					ImVec4(0.044f, 0.042f, 0.039f, 1.0f),
					ImVec4(0.070f, 0.066f, 0.058f, 1.0f),
					ImVec4(0.058f, 0.055f, 0.050f, 1.0f),
					ImVec4(0.820f, 0.580f, 0.320f, 1.0f),
					ImVec4(0.910f, 0.670f, 0.390f, 1.0f),
					ImVec4(0.980f, 0.760f, 0.480f, 1.0f),
					ImVec4(0.074f, 0.070f, 0.063f, 1.0f),
					ImVec4(0.145f, 0.125f, 0.095f, 1.0f),
					ImVec4(0.128f, 0.095f, 0.060f, 1.0f),
					ImVec4(0.088f, 0.082f, 0.072f, 1.0f),
					ImVec4(0.860f, 0.620f, 0.340f, 1.0f)
				};
			}
		}

		void apply_shared_style_metrics(ImGuiStyle& style)
		{
			style.WindowPadding = ImVec2(10.0f, 8.0f);
			style.WindowRounding = 5.0f;
			style.ChildRounding = 5.0f;
			style.PopupRounding = 5.0f;
			style.FramePadding = ImVec2(8.0f, 5.0f);
			style.FrameRounding = 4.0f;
			style.ItemSpacing = ImVec2(8.0f, 6.0f);
			style.ItemInnerSpacing = ImVec2(6.0f, 5.0f);
			style.CellPadding = ImVec2(8.0f, 4.0f);
			style.ScrollbarSize = 12.0f;
			style.ScrollbarRounding = 6.0f;
			style.GrabRounding = 4.0f;
			style.TabRounding = 4.0f;
			style.DockingSeparatorSize = 1.0f;
		}

		void apply_dark_palette(const theme_palette& palette)
		{
			ImGuiStyle& style = ImGui::GetStyle();
			ImGui::StyleColorsDark();
			apply_shared_style_metrics(style);

			ImVec4* colors = style.Colors;
			colors[ImGuiCol_Text] = palette.text;
			colors[ImGuiCol_TextDisabled] = palette.text_disabled;
			colors[ImGuiCol_WindowBg] = palette.window;
			colors[ImGuiCol_ChildBg] = palette.child;
			colors[ImGuiCol_PopupBg] = palette.popup;
			colors[ImGuiCol_Border] = palette.border;
			colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
			colors[ImGuiCol_FrameBg] = palette.frame;
			colors[ImGuiCol_FrameBgHovered] = palette.frame_hovered;
			colors[ImGuiCol_FrameBgActive] = palette.frame_active;
			colors[ImGuiCol_TitleBg] = palette.title;
			colors[ImGuiCol_TitleBgActive] = palette.title_active;
			colors[ImGuiCol_TitleBgCollapsed] = palette.title;
			colors[ImGuiCol_MenuBarBg] = palette.menu;
			colors[ImGuiCol_ScrollbarBg] = palette.title;
			colors[ImGuiCol_ScrollbarGrab] = palette.frame_hovered;
			colors[ImGuiCol_ScrollbarGrabHovered] = palette.border;
			colors[ImGuiCol_ScrollbarGrabActive] = palette.accent_hovered;
			colors[ImGuiCol_CheckMark] = palette.accent;
			colors[ImGuiCol_SliderGrab] = palette.accent;
			colors[ImGuiCol_SliderGrabActive] = palette.accent_active;
			colors[ImGuiCol_Button] = palette.frame;
			colors[ImGuiCol_ButtonHovered] = palette.frame_hovered;
			colors[ImGuiCol_ButtonActive] = palette.frame_active;
			colors[ImGuiCol_Header] = palette.frame;
			colors[ImGuiCol_HeaderHovered] = palette.frame_hovered;
			colors[ImGuiCol_HeaderActive] = palette.frame_active;
			colors[ImGuiCol_Separator] = palette.border;
			colors[ImGuiCol_SeparatorHovered] = palette.accent;
			colors[ImGuiCol_SeparatorActive] = palette.accent_active;
			colors[ImGuiCol_ResizeGrip] = ImVec4(palette.accent.x, palette.accent.y, palette.accent.z, 0.22f);
			colors[ImGuiCol_ResizeGripHovered] = ImVec4(palette.accent_hovered.x, palette.accent_hovered.y, palette.accent_hovered.z, 0.50f);
			colors[ImGuiCol_ResizeGripActive] = ImVec4(palette.accent_active.x, palette.accent_active.y, palette.accent_active.z, 0.82f);
			colors[ImGuiCol_Tab] = palette.tab;
			colors[ImGuiCol_TabHovered] = palette.tab_hovered;
			colors[ImGuiCol_TabActive] = palette.tab_active;
			colors[ImGuiCol_TabUnfocused] = palette.title;
			colors[ImGuiCol_TabUnfocusedActive] = palette.frame;
			colors[ImGuiCol_DockingPreview] = ImVec4(palette.accent.x, palette.accent.y, palette.accent.z, 0.42f);
			colors[ImGuiCol_DockingEmptyBg] = palette.title;
			colors[ImGuiCol_PlotLines] = palette.text_disabled;
			colors[ImGuiCol_PlotLinesHovered] = palette.accent_hovered;
			colors[ImGuiCol_PlotHistogram] = palette.warning;
			colors[ImGuiCol_PlotHistogramHovered] = palette.accent_active;
			colors[ImGuiCol_TableHeaderBg] = palette.table_header;
			colors[ImGuiCol_TableBorderStrong] = palette.border;
			colors[ImGuiCol_TableBorderLight] = ImVec4(palette.border.x, palette.border.y, palette.border.z, 0.72f);
			colors[ImGuiCol_TableRowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
			colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.0f, 0.92f, 0.78f, 0.035f);
			colors[ImGuiCol_TextSelectedBg] = ImVec4(palette.accent.x, palette.accent.y, palette.accent.z, 0.30f);
			colors[ImGuiCol_DragDropTarget] = ImVec4(palette.warning.x, palette.warning.y, palette.warning.z, 0.88f);
			colors[ImGuiCol_NavHighlight] = palette.accent;
			colors[ImGuiCol_NavWindowingHighlight] = ImVec4(0.92f, 0.90f, 0.84f, 0.70f);
			colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.35f);
			colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.62f);
		}

		void apply_light_theme()
		{
			ImGuiStyle& style = ImGui::GetStyle();
			ImGui::StyleColorsLight();
			apply_shared_style_metrics(style);

			ImVec4* colors = style.Colors;
			colors[ImGuiCol_Text] = ImVec4(0.17f, 0.15f, 0.13f, 1.0f);
			colors[ImGuiCol_TextDisabled] = ImVec4(0.55f, 0.51f, 0.46f, 1.0f);
			colors[ImGuiCol_WindowBg] = ImVec4(0.930f, 0.915f, 0.885f, 0.98f);
			colors[ImGuiCol_ChildBg] = ImVec4(0.965f, 0.952f, 0.925f, 1.0f);
			colors[ImGuiCol_PopupBg] = ImVec4(0.980f, 0.966f, 0.940f, 0.99f);
			colors[ImGuiCol_Border] = ImVec4(0.715f, 0.675f, 0.600f, 1.0f);
			colors[ImGuiCol_FrameBg] = ImVec4(0.885f, 0.860f, 0.810f, 1.0f);
			colors[ImGuiCol_FrameBgHovered] = ImVec4(0.825f, 0.790f, 0.720f, 1.0f);
			colors[ImGuiCol_FrameBgActive] = ImVec4(0.770f, 0.665f, 0.510f, 1.0f);
			colors[ImGuiCol_TitleBg] = ImVec4(0.865f, 0.835f, 0.780f, 1.0f);
			colors[ImGuiCol_TitleBgActive] = ImVec4(0.815f, 0.765f, 0.685f, 1.0f);
			colors[ImGuiCol_TitleBgCollapsed] = colors[ImGuiCol_TitleBg];
			colors[ImGuiCol_MenuBarBg] = ImVec4(0.875f, 0.845f, 0.790f, 1.0f);
			colors[ImGuiCol_CheckMark] = ImVec4(0.560f, 0.385f, 0.185f, 1.0f);
			colors[ImGuiCol_SliderGrab] = ImVec4(0.650f, 0.460f, 0.250f, 1.0f);
			colors[ImGuiCol_SliderGrabActive] = ImVec4(0.740f, 0.520f, 0.270f, 1.0f);
			colors[ImGuiCol_Button] = ImVec4(0.850f, 0.815f, 0.755f, 1.0f);
			colors[ImGuiCol_ButtonHovered] = ImVec4(0.790f, 0.740f, 0.660f, 1.0f);
			colors[ImGuiCol_ButtonActive] = ImVec4(0.700f, 0.575f, 0.415f, 1.0f);
			colors[ImGuiCol_Header] = ImVec4(0.860f, 0.830f, 0.770f, 1.0f);
			colors[ImGuiCol_HeaderHovered] = ImVec4(0.790f, 0.745f, 0.665f, 1.0f);
			colors[ImGuiCol_HeaderActive] = ImVec4(0.710f, 0.585f, 0.430f, 1.0f);
			colors[ImGuiCol_Separator] = colors[ImGuiCol_Border];
			colors[ImGuiCol_SeparatorHovered] = ImVec4(0.650f, 0.460f, 0.250f, 1.0f);
			colors[ImGuiCol_SeparatorActive] = ImVec4(0.740f, 0.520f, 0.270f, 1.0f);
			colors[ImGuiCol_Tab] = ImVec4(0.820f, 0.785f, 0.720f, 1.0f);
			colors[ImGuiCol_TabHovered] = ImVec4(0.760f, 0.700f, 0.600f, 1.0f);
			colors[ImGuiCol_TabActive] = ImVec4(0.925f, 0.900f, 0.850f, 1.0f);
			colors[ImGuiCol_TabUnfocused] = ImVec4(0.855f, 0.825f, 0.765f, 1.0f);
			colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.900f, 0.870f, 0.810f, 1.0f);
			colors[ImGuiCol_DockingPreview] = ImVec4(0.650f, 0.460f, 0.250f, 0.35f);
			colors[ImGuiCol_TextSelectedBg] = ImVec4(0.740f, 0.520f, 0.270f, 0.24f);
			colors[ImGuiCol_DragDropTarget] = ImVec4(0.740f, 0.520f, 0.270f, 0.86f);
			colors[ImGuiCol_NavHighlight] = ImVec4(0.650f, 0.460f, 0.250f, 1.0f);
		}

		void draw_settings_heading(const char* title)
		{
			ImGui::Spacing();
			ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_CheckMark), "%s", title);
			ImGui::Separator();
			ImGui::Spacing();
		}
	}

	void apply_editor_theme(editor_theme theme)
	{
		if (theme == editor_theme::light)
		{
			apply_light_theme();
			return;
		}

		apply_dark_palette(get_dark_palette(theme));
	}

	UI_settings::UI_settings()
	{
		reset_shortcuts_to_default();
	}

	void UI_settings::reset_shortcuts_to_default()
	{
		m_shortcuts = {};
		m_shortcuts[static_cast<size_t>(editor_shortcut_action::NewScene)] = { key::N, true, false, false };
		m_shortcuts[static_cast<size_t>(editor_shortcut_action::OpenProject)] = { key::O, true, false, false };
		m_shortcuts[static_cast<size_t>(editor_shortcut_action::SaveScene)] = { key::S, true, false, false };
		m_shortcuts[static_cast<size_t>(editor_shortcut_action::SaveSceneAs)] = { key::S, true, true, false };
		m_shortcuts[static_cast<size_t>(editor_shortcut_action::SaveProject)] = { key::S, true, false, true };
		m_shortcuts[static_cast<size_t>(editor_shortcut_action::CloseScene)] = { key::W, true, false, false };
		m_shortcuts[static_cast<size_t>(editor_shortcut_action::ReloadScripts)] = { key::R, true, false, false };
		m_shortcuts[static_cast<size_t>(editor_shortcut_action::DuplicateEntity)] = { key::D, true, false, false };
		m_shortcuts[static_cast<size_t>(editor_shortcut_action::DeleteEntity)] = { key::del, false, false, false };
		m_shortcuts[static_cast<size_t>(editor_shortcut_action::Undo)] = { key::Z, true, false, false };
		m_shortcuts[static_cast<size_t>(editor_shortcut_action::Redo)] = { key::Y, true, false, false };
		m_shortcuts[static_cast<size_t>(editor_shortcut_action::SelectAll)] = { key::A, true, false, false };
		m_shortcuts[static_cast<size_t>(editor_shortcut_action::Copy)] = { key::C, true, false, false };
		m_shortcuts[static_cast<size_t>(editor_shortcut_action::Paste)] = { key::V, true, false, false };
		m_shortcuts[static_cast<size_t>(editor_shortcut_action::Cut)] = { key::X, true, false, false };
		m_shortcuts[static_cast<size_t>(editor_shortcut_action::Play)] = { key::F5, false, false, false };
		m_shortcuts[static_cast<size_t>(editor_shortcut_action::Simulate)] = { key::F5, true, false, false };
		m_shortcuts[static_cast<size_t>(editor_shortcut_action::Stop)] = { key::escape, false, false, false };
		m_shortcuts[static_cast<size_t>(editor_shortcut_action::Pause)] = { key::F6, false, false, false };
		m_shortcuts[static_cast<size_t>(editor_shortcut_action::GizmoNone)] = { key::Q, false, false, false };
		m_shortcuts[static_cast<size_t>(editor_shortcut_action::GizmoTranslate)] = { key::W, false, false, false };
		m_shortcuts[static_cast<size_t>(editor_shortcut_action::GizmoRotate)] = { key::E, false, false, false };
		m_shortcuts[static_cast<size_t>(editor_shortcut_action::GizmoScale)] = { key::R, false, false, false };
		m_shortcuts[static_cast<size_t>(editor_shortcut_action::OpenSettings)] = { key::comma, true, false, false };
		m_shortcuts[static_cast<size_t>(editor_shortcut_action::OpenCommandPalette)] = { key::P, true, true, false };
		m_shortcuts_initialized = true;
	}

	bool UI_settings::shortcut_matches(editor_shortcut_action action, key_code key_in, bool ctrl, bool shift, bool alt) const
	{
		const size_t index = static_cast<size_t>(action);
		if (index >= m_shortcuts.size() || has_shortcut_conflict(index))
			return false;
		const shortcut_binding& binding = m_shortcuts[index];
		return binding.key == key_in && binding.ctrl == ctrl && binding.shift == shift && binding.alt == alt;
	}

	bool UI_settings::has_shortcut_conflict(editor_shortcut_action action) const
	{
		return has_shortcut_conflict(static_cast<size_t>(action));
	}

	bool UI_settings::has_shortcut_conflict(size_t index) const
	{
		for (size_t i = 0; i < m_shortcuts.size(); ++i)
			if (i != index && same_binding(m_shortcuts[index], m_shortcuts[i]))
				return true;
		return false;
	}

	std::string UI_settings::shortcut_label(const shortcut_binding& binding) const
	{
		std::string label;
		if (binding.ctrl)
			label += "Ctrl+";
		if (binding.shift)
			label += "Shift+";
		if (binding.alt)
			label += "Alt+";
		label += key::to_string(binding.key);
		return label;
	}

	std::string UI_settings::get_shortcut_label(editor_shortcut_action action) const
	{
		const size_t index = static_cast<size_t>(action);
		if (index >= m_shortcuts.size())
			return {};
		return shortcut_label(m_shortcuts[index]);
	}

	shortcut_binding UI_settings::get_shortcut_binding(editor_shortcut_action action) const
	{
		const size_t index = static_cast<size_t>(action);
		if (index >= m_shortcuts.size())
			return {};
		return m_shortcuts[index];
	}

	void UI_settings::set_shortcut_binding(editor_shortcut_action action, const shortcut_binding& binding)
	{
		const size_t index = static_cast<size_t>(action);
		if (index >= m_shortcuts.size())
			return;
		m_shortcuts[index] = binding;
		m_shortcuts_initialized = true;
		mark_dirty();
	}

	void UI_settings::set_show_physics_colliders(bool value)
	{
		m_show_physics_colliders = value;
		mark_dirty();
	}

	void UI_settings::set_step_frame(int value)
	{
		m_step_frame = value < 1 ? 1 : value;
		mark_dirty();
	}

	void UI_settings::set_snap_values(uint32_t idx, const glm::vec3& value)
	{
		if (idx >= 3)
			return;
		m_snap_values[idx] = value;
		mark_dirty();
	}

	void UI_settings::set_theme(editor_theme theme)
	{
		m_theme = theme;
		apply_editor_theme(theme);
		mark_dirty();
	}

	bool UI_settings::consume_dirty()
	{
		bool dirty = m_dirty;
		m_dirty = false;
		return dirty;
	}

	const char* UI_settings::get_action_display_name(editor_shortcut_action action)
	{
		switch (action)
		{
		case editor_shortcut_action::NewScene: return "New Scene";
		case editor_shortcut_action::OpenProject: return "Open Project";
		case editor_shortcut_action::SaveScene: return "Save Scene";
		case editor_shortcut_action::SaveSceneAs: return "Save Scene As";
		case editor_shortcut_action::SaveProject: return "Save Project";
		case editor_shortcut_action::CloseScene: return "Close Scene";
		case editor_shortcut_action::ReloadScripts: return "Reload Scripts";
		case editor_shortcut_action::DuplicateEntity: return "Duplicate Entity";
		case editor_shortcut_action::DeleteEntity: return "Delete Entity";
		case editor_shortcut_action::Undo: return "Undo";
		case editor_shortcut_action::Redo: return "Redo";
		case editor_shortcut_action::SelectAll: return "Select All";
		case editor_shortcut_action::Copy: return "Copy";
		case editor_shortcut_action::Paste: return "Paste";
		case editor_shortcut_action::Cut: return "Cut";
		case editor_shortcut_action::Play: return "Play";
		case editor_shortcut_action::Simulate: return "Simulate";
		case editor_shortcut_action::Stop: return "Stop";
		case editor_shortcut_action::Pause: return "Pause";
		case editor_shortcut_action::GizmoNone: return "Select Tool";
		case editor_shortcut_action::GizmoTranslate: return "Move Tool";
		case editor_shortcut_action::GizmoRotate: return "Rotate Tool";
		case editor_shortcut_action::GizmoScale: return "Scale Tool";
		case editor_shortcut_action::OpenSettings: return "Open Settings";
		case editor_shortcut_action::OpenCommandPalette: return "Command Palette";
		default: return "Unknown";
		}
	}

	const char* UI_settings::get_action_storage_key(editor_shortcut_action action)
	{
		switch (action)
		{
		case editor_shortcut_action::NewScene: return "new_scene";
		case editor_shortcut_action::OpenProject: return "open_project";
		case editor_shortcut_action::SaveScene: return "save_scene";
		case editor_shortcut_action::SaveSceneAs: return "save_scene_as";
		case editor_shortcut_action::SaveProject: return "save_project";
		case editor_shortcut_action::CloseScene: return "close_scene";
		case editor_shortcut_action::ReloadScripts: return "reload_scripts";
		case editor_shortcut_action::DuplicateEntity: return "duplicate_entity";
		case editor_shortcut_action::DeleteEntity: return "delete_entity";
		case editor_shortcut_action::Undo: return "undo";
		case editor_shortcut_action::Redo: return "redo";
		case editor_shortcut_action::SelectAll: return "select_all";
		case editor_shortcut_action::Copy: return "copy";
		case editor_shortcut_action::Paste: return "paste";
		case editor_shortcut_action::Cut: return "cut";
		case editor_shortcut_action::Play: return "play";
		case editor_shortcut_action::Simulate: return "simulate";
		case editor_shortcut_action::Stop: return "stop";
		case editor_shortcut_action::Pause: return "pause";
		case editor_shortcut_action::GizmoNone: return "gizmo_none";
		case editor_shortcut_action::GizmoTranslate: return "gizmo_translate";
		case editor_shortcut_action::GizmoRotate: return "gizmo_rotate";
		case editor_shortcut_action::GizmoScale: return "gizmo_scale";
		case editor_shortcut_action::OpenSettings: return "open_settings";
		case editor_shortcut_action::OpenCommandPalette: return "open_command_palette";
		default: return "unknown";
		}
	}

	const char* UI_settings::get_theme_name(editor_theme theme)
	{
		switch (theme)
		{
		case editor_theme::whip_dark: return "Carbon";
		case editor_theme::graphite: return "Graphite";
		case editor_theme::ember: return "Ember";
		case editor_theme::moss: return "Moss";
		case editor_theme::light: return "Porcelain";
		default: return "Carbon";
		}
	}

	const char* UI_settings::get_theme_description(editor_theme theme)
	{
		switch (theme)
		{
		case editor_theme::whip_dark: return "Warm charcoal, quiet amber highlights";
		case editor_theme::graphite: return "Neutral studio graphite with brass accents";
		case editor_theme::ember: return "Deep brown-black surfaces with copper focus";
		case editor_theme::moss: return "Soft green-black panels with muted moss accents";
		case editor_theme::light: return "Warm light workspace for bright rooms";
		default: return "Warm charcoal, quiet amber highlights";
		}
	}

	const char* UI_settings::get_action_category(editor_shortcut_action action)
	{
		switch (action)
		{
		case editor_shortcut_action::NewScene:
		case editor_shortcut_action::OpenProject:
		case editor_shortcut_action::SaveScene:
		case editor_shortcut_action::SaveSceneAs:
		case editor_shortcut_action::SaveProject:
		case editor_shortcut_action::CloseScene:
			return "File";
		case editor_shortcut_action::Undo:
		case editor_shortcut_action::Redo:
		case editor_shortcut_action::SelectAll:
		case editor_shortcut_action::Copy:
		case editor_shortcut_action::Paste:
		case editor_shortcut_action::Cut:
		case editor_shortcut_action::DuplicateEntity:
		case editor_shortcut_action::DeleteEntity:
			return "Edit";
		case editor_shortcut_action::Play:
		case editor_shortcut_action::Simulate:
		case editor_shortcut_action::Stop:
		case editor_shortcut_action::Pause:
			return "Run";
		case editor_shortcut_action::GizmoNone:
		case editor_shortcut_action::GizmoTranslate:
		case editor_shortcut_action::GizmoRotate:
		case editor_shortcut_action::GizmoScale:
			return "Tools";
		case editor_shortcut_action::ReloadScripts:
			return "Script";
		case editor_shortcut_action::OpenSettings:
		case editor_shortcut_action::OpenCommandPalette:
			return "Window";
		default:
			return "General";
		}
	}

	void UI_settings::draw_general_settings()
	{
		draw_settings_heading("Viewport");
		if (ImGui::Checkbox("Show physics colliders", &m_show_physics_colliders))
			mark_dirty();
		ImGui::SetNextItemWidth(140.0f);
		if (ImGui::InputInt("Step Frame", &m_step_frame))
			mark_dirty();
		if (m_step_frame < 1)
			m_step_frame = 1;

		draw_settings_heading("Gizmo Snap");
		UI::draw_vec3_control("Translation", m_snap_values[0]);
		if (ImGui::IsItemDeactivatedAfterEdit())
			mark_dirty();
		UI::draw_vec3_control("Rotation", m_snap_values[1]);
		if (ImGui::IsItemDeactivatedAfterEdit())
			mark_dirty();
		UI::draw_vec3_control("Scale", m_snap_values[2]);
		if (ImGui::IsItemDeactivatedAfterEdit())
			mark_dirty();
	}

	void UI_settings::draw_appearance_settings()
	{
		draw_settings_heading("Appearance");
		const char* current_theme = get_theme_name(m_theme);
		if (ImGui::BeginCombo("Theme", current_theme))
		{
			for (editor_theme theme : { editor_theme::whip_dark, editor_theme::graphite, editor_theme::ember, editor_theme::moss, editor_theme::light })
			{
				const bool selected = m_theme == theme;
				if (ImGui::Selectable(get_theme_name(theme), selected))
					set_theme(theme);
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("%s", get_theme_description(theme));
				if (selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		ImGui::TextDisabled("%s", get_theme_description(m_theme));
	}

	void UI_settings::draw_shortcut_settings()
	{
		draw_settings_heading("Shortcut Map");
		if (ImGui::Button("Reset Defaults", ImVec2(128.0f, 0.0f)))
			reset_shortcuts_to_default();

		ImGui::Spacing();
		if (ImGui::BeginTable("##ShortcutTable", 7, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable))
		{
			ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 160.0f);
			ImGui::TableSetupColumn("Group", ImGuiTableColumnFlags_WidthFixed, 80.0f);
			ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Ctrl", ImGuiTableColumnFlags_WidthFixed, 58.0f);
			ImGui::TableSetupColumn("Shift", ImGuiTableColumnFlags_WidthFixed, 58.0f);
			ImGui::TableSetupColumn("Alt", ImGuiTableColumnFlags_WidthFixed, 58.0f);
			ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 90.0f);
			ImGui::TableHeadersRow();

			for (size_t i = 0; i < m_shortcuts.size(); ++i)
			{
				shortcut_binding& binding = m_shortcuts[i];
				const bool conflict = has_shortcut_conflict(i);
				ImGui::PushID(static_cast<int>(i));
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(get_action_display_name(static_cast<editor_shortcut_action>(i)));
				ImGui::TableNextColumn();
				ImGui::TextDisabled("%s", get_action_category(static_cast<editor_shortcut_action>(i)));
				ImGui::TableNextColumn();
				ImGui::SetNextItemWidth(-1.0f);
				if (ImGui::BeginCombo("##Key", key::to_string(binding.key)))
				{
					for (key_code candidate : editable_keys)
					{
						const bool selected = binding.key == candidate;
						if (ImGui::Selectable(key::to_string(candidate), selected))
						{
							binding.key = candidate;
							mark_dirty();
						}
						if (selected)
							ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}
				ImGui::TableNextColumn();
				if (ImGui::Checkbox("##Ctrl", &binding.ctrl))
					mark_dirty();
				ImGui::TableNextColumn();
				if (ImGui::Checkbox("##Shift", &binding.shift))
					mark_dirty();
				ImGui::TableNextColumn();
				if (ImGui::Checkbox("##Alt", &binding.alt))
					mark_dirty();
				ImGui::TableNextColumn();
				if (conflict)
					ImGui::TextColored(ImVec4(0.95f, 0.50f, 0.34f, 1.0f), "Conflict");
				else
					ImGui::TextDisabled("%s", shortcut_label(binding).c_str());
				ImGui::PopID();
			}
			ImGui::EndTable();
		}
	}

	void UI_settings::draw_content()
	{
		if (!m_shortcuts_initialized)
			reset_shortcuts_to_default();

		draw_general_settings();
		ImGui::Spacing();
		draw_appearance_settings();
		ImGui::Spacing();
		draw_shortcut_settings();
	}

	void UI_settings::mark_dirty()
	{
		m_dirty = true;
	}

	void UI_settings::on_imgui_render()
	{
		if (!m_shortcuts_initialized)
			reset_shortcuts_to_default();

		if (m_open)
		{
			ImGui::SetNextWindowSize(ImVec2(740.0f, 520.0f), ImGuiCond_FirstUseEver);
			ImGui::Begin("Settings", &m_open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking);
			draw_content();
			ImGui::End();
		}
	}
}

_WHIP_END
