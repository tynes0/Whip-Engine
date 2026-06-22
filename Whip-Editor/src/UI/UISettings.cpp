#include "WhipPch.h"
#include <Whip-Editor/UI/UISettings.h>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <Whip-Editor/Managers/EditorShortcutManager.h>
#include <Whip-Editor/UI/UIHelpers.h>

_WHIP_START

namespace UI
{
	namespace
	{
		struct ThemePalette
		{
			ImVec4 m_Text;
			ImVec4 m_TextDisabled;
			ImVec4 m_Window;
			ImVec4 m_Child;
			ImVec4 m_Popup;
			ImVec4 m_Border;
			ImVec4 m_Frame;
			ImVec4 m_FrameHovered;
			ImVec4 m_FrameActive;
			ImVec4 m_Title;
			ImVec4 m_TitleActive;
			ImVec4 m_Menu;
			ImVec4 m_Accent;
			ImVec4 m_AccentHovered;
			ImVec4 m_AccentActive;
			ImVec4 m_Tab;
			ImVec4 m_TabHovered;
			ImVec4 m_TabActive;
			ImVec4 m_TableHeader;
			ImVec4 m_Warning;
		};

		constexpr KeyCode EditableKeys[] =
		{
			Key::Q, Key::W, Key::E, Key::R, Key::T, Key::Y, Key::A, Key::S, Key::D, Key::F, Key::G, Key::Z, Key::X, Key::C, Key::V, Key::B, Key::N, Key::O, Key::P,
			Key::D0, Key::D1, Key::D2, Key::D3, Key::D4, Key::D5, Key::D6, Key::D7, Key::D8, Key::D9,
			Key::F1, Key::F2, Key::F3, Key::F4, Key::F5, Key::F6, Key::F7, Key::F8, Key::F9, Key::F10, Key::F11, Key::F12,
			Key::Escape, Key::Delete, Key::Backspace, Key::Insert, Key::Space, Key::Enter, Key::Tab,
			Key::Left, Key::Right, Key::Up, Key::Down, Key::Home, Key::End,
			Key::Comma, Key::Period, Key::Minus, Key::Equal, Key::LeftBracket, Key::RightBracket, Key::GraveAccent
		};

		bool SameBinding(const ShortcutBinding& left, const ShortcutBinding& right)
		{
			return left.m_Key != 0 &&
				left.m_Key == right.m_Key &&
				left.m_Ctrl == right.m_Ctrl &&
				left.m_Shift == right.m_Shift &&
				left.m_Alt == right.m_Alt;
		}

		ThemePalette GetDarkPalette(EditorTheme theme)
		{
			switch (theme)
			{
			case EditorTheme::Graphite:
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
			case EditorTheme::Ember:
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
			case EditorTheme::Moss:
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
			case EditorTheme::WhipDark:
			default:
				return {
					ImVec4(0.92f, 0.94f, 0.96f, 1.0f),
					ImVec4(0.55f, 0.60f, 0.65f, 1.0f),
					ImVec4(0.035f, 0.046f, 0.056f, 0.98f),
					ImVec4(0.052f, 0.067f, 0.081f, 1.0f),
					ImVec4(0.045f, 0.058f, 0.070f, 0.99f),
					ImVec4(0.175f, 0.220f, 0.265f, 1.0f),
					ImVec4(0.080f, 0.102f, 0.122f, 1.0f),
					ImVec4(0.120f, 0.150f, 0.178f, 1.0f),
					ImVec4(0.195f, 0.235f, 0.275f, 1.0f),
					ImVec4(0.027f, 0.036f, 0.046f, 1.0f),
					ImVec4(0.055f, 0.073f, 0.090f, 1.0f),
					ImVec4(0.040f, 0.055f, 0.070f, 1.0f),
					ImVec4(0.700f, 0.770f, 0.850f, 1.0f),
					ImVec4(0.820f, 0.880f, 0.950f, 1.0f),
					ImVec4(0.950f, 0.970f, 1.000f, 1.0f),
					ImVec4(0.060f, 0.078f, 0.094f, 1.0f),
					ImVec4(0.125f, 0.160f, 0.195f, 1.0f),
					ImVec4(0.098f, 0.132f, 0.166f, 1.0f),
					ImVec4(0.067f, 0.085f, 0.102f, 1.0f),
					ImVec4(0.920f, 0.650f, 0.360f, 1.0f)
				};
			}
		}

		void ApplySharedStyleMetrics(ImGuiStyle& style)
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

		void ApplyDarkPalette(const ThemePalette& palette)
		{
			ImGuiStyle& style = ImGui::GetStyle();
			ImGui::StyleColorsDark();
			ApplySharedStyleMetrics(style);

			ImVec4* colors = style.Colors;
			colors[ImGuiCol_Text] = palette.m_Text;
			colors[ImGuiCol_TextDisabled] = palette.m_TextDisabled;
			colors[ImGuiCol_WindowBg] = palette.m_Window;
			colors[ImGuiCol_ChildBg] = palette.m_Child;
			colors[ImGuiCol_PopupBg] = palette.m_Popup;
			colors[ImGuiCol_Border] = palette.m_Border;
			colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
			colors[ImGuiCol_FrameBg] = palette.m_Frame;
			colors[ImGuiCol_FrameBgHovered] = palette.m_FrameHovered;
			colors[ImGuiCol_FrameBgActive] = palette.m_FrameActive;
			colors[ImGuiCol_TitleBg] = palette.m_Title;
			colors[ImGuiCol_TitleBgActive] = palette.m_TitleActive;
			colors[ImGuiCol_TitleBgCollapsed] = palette.m_Title;
			colors[ImGuiCol_MenuBarBg] = palette.m_Menu;
			colors[ImGuiCol_ScrollbarBg] = palette.m_Title;
			colors[ImGuiCol_ScrollbarGrab] = palette.m_FrameHovered;
			colors[ImGuiCol_ScrollbarGrabHovered] = palette.m_Border;
			colors[ImGuiCol_ScrollbarGrabActive] = palette.m_AccentHovered;
			colors[ImGuiCol_CheckMark] = palette.m_Accent;
			colors[ImGuiCol_SliderGrab] = palette.m_Accent;
			colors[ImGuiCol_SliderGrabActive] = palette.m_AccentActive;
			colors[ImGuiCol_Button] = palette.m_Frame;
			colors[ImGuiCol_ButtonHovered] = palette.m_FrameHovered;
			colors[ImGuiCol_ButtonActive] = palette.m_FrameActive;
			colors[ImGuiCol_Header] = palette.m_Frame;
			colors[ImGuiCol_HeaderHovered] = palette.m_FrameHovered;
			colors[ImGuiCol_HeaderActive] = palette.m_FrameActive;
			colors[ImGuiCol_Separator] = palette.m_Border;
			colors[ImGuiCol_SeparatorHovered] = palette.m_Accent;
			colors[ImGuiCol_SeparatorActive] = palette.m_AccentActive;
			colors[ImGuiCol_ResizeGrip] = ImVec4(palette.m_Accent.x, palette.m_Accent.y, palette.m_Accent.z, 0.22f);
			colors[ImGuiCol_ResizeGripHovered] = ImVec4(palette.m_AccentHovered.x, palette.m_AccentHovered.y, palette.m_AccentHovered.z, 0.50f);
			colors[ImGuiCol_ResizeGripActive] = ImVec4(palette.m_AccentActive.x, palette.m_AccentActive.y, palette.m_AccentActive.z, 0.82f);
			colors[ImGuiCol_Tab] = palette.m_Tab;
			colors[ImGuiCol_TabHovered] = palette.m_TabHovered;
			colors[ImGuiCol_TabActive] = palette.m_TabActive;
			colors[ImGuiCol_TabUnfocused] = palette.m_Title;
			colors[ImGuiCol_TabUnfocusedActive] = palette.m_Frame;
			colors[ImGuiCol_DockingPreview] = ImVec4(palette.m_Accent.x, palette.m_Accent.y, palette.m_Accent.z, 0.42f);
			colors[ImGuiCol_DockingEmptyBg] = palette.m_Title;
			colors[ImGuiCol_PlotLines] = palette.m_TextDisabled;
			colors[ImGuiCol_PlotLinesHovered] = palette.m_AccentHovered;
			colors[ImGuiCol_PlotHistogram] = palette.m_Warning;
			colors[ImGuiCol_PlotHistogramHovered] = palette.m_AccentActive;
			colors[ImGuiCol_TableHeaderBg] = palette.m_TableHeader;
			colors[ImGuiCol_TableBorderStrong] = palette.m_Border;
			colors[ImGuiCol_TableBorderLight] = ImVec4(palette.m_Border.x, palette.m_Border.y, palette.m_Border.z, 0.72f);
			colors[ImGuiCol_TableRowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
			colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.0f, 0.92f, 0.78f, 0.035f);
			colors[ImGuiCol_TextSelectedBg] = ImVec4(palette.m_Accent.x, palette.m_Accent.y, palette.m_Accent.z, 0.30f);
			colors[ImGuiCol_DragDropTarget] = ImVec4(palette.m_Warning.x, palette.m_Warning.y, palette.m_Warning.z, 0.88f);
			colors[ImGuiCol_NavHighlight] = palette.m_Accent;
			colors[ImGuiCol_NavWindowingHighlight] = ImVec4(0.92f, 0.90f, 0.84f, 0.70f);
			colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.35f);
			colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.62f);
		}

		void ApplyLightTheme()
		{
			ImGuiStyle& style = ImGui::GetStyle();
			ImGui::StyleColorsLight();
			ApplySharedStyleMetrics(style);

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

		void DrawSettingsHeading(const char* title)
		{
			ImGui::Spacing();
			ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_CheckMark), "%s", title);
			ImGui::Separator();
			ImGui::Spacing();
		}
	}

	void ApplyEditorTheme(EditorTheme theme)
	{
		if (theme == EditorTheme::Light)
		{
			ApplyLightTheme();
			return;
		}

		ApplyDarkPalette(GetDarkPalette(theme));
	}

	UISettings::UISettings()
	{
		ResetShortcutsToDefault();
	}

	void UISettings::ResetShortcutsToDefault()
	{
		m_Shortcuts = {};
		m_Shortcuts[static_cast<size_t>(EditorShortcutAction::NewScene)] = { Key::N, true, false, false };
		m_Shortcuts[static_cast<size_t>(EditorShortcutAction::OpenProject)] = { Key::O, true, false, false };
		m_Shortcuts[static_cast<size_t>(EditorShortcutAction::SaveScene)] = { Key::S, true, false, false };
		m_Shortcuts[static_cast<size_t>(EditorShortcutAction::SaveSceneAs)] = { Key::S, true, true, false };
		m_Shortcuts[static_cast<size_t>(EditorShortcutAction::SaveProject)] = { Key::S, true, false, true };
		m_Shortcuts[static_cast<size_t>(EditorShortcutAction::CloseScene)] = { Key::W, true, false, false };
		m_Shortcuts[static_cast<size_t>(EditorShortcutAction::ReloadScripts)] = { Key::R, true, false, false };
		m_Shortcuts[static_cast<size_t>(EditorShortcutAction::DuplicateEntity)] = { Key::D, true, false, false };
		m_Shortcuts[static_cast<size_t>(EditorShortcutAction::DeleteEntity)] = { Key::Delete, false, false, false };
		m_Shortcuts[static_cast<size_t>(EditorShortcutAction::Undo)] = { Key::Z, true, false, false };
		m_Shortcuts[static_cast<size_t>(EditorShortcutAction::Redo)] = { Key::Y, true, false, false };
		m_Shortcuts[static_cast<size_t>(EditorShortcutAction::SelectAll)] = { Key::A, true, false, false };
		m_Shortcuts[static_cast<size_t>(EditorShortcutAction::Copy)] = { Key::C, true, false, false };
		m_Shortcuts[static_cast<size_t>(EditorShortcutAction::Paste)] = { Key::V, true, false, false };
		m_Shortcuts[static_cast<size_t>(EditorShortcutAction::Cut)] = { Key::X, true, false, false };
		m_Shortcuts[static_cast<size_t>(EditorShortcutAction::Play)] = { Key::F5, false, false, false };
		m_Shortcuts[static_cast<size_t>(EditorShortcutAction::Simulate)] = { Key::F5, true, false, false };
		m_Shortcuts[static_cast<size_t>(EditorShortcutAction::Stop)] = { Key::Escape, false, false, false };
		m_Shortcuts[static_cast<size_t>(EditorShortcutAction::Pause)] = { Key::F6, false, false, false };
		m_Shortcuts[static_cast<size_t>(EditorShortcutAction::GizmoNone)] = { Key::Q, false, false, false };
		m_Shortcuts[static_cast<size_t>(EditorShortcutAction::GizmoTranslate)] = { Key::W, false, false, false };
		m_Shortcuts[static_cast<size_t>(EditorShortcutAction::GizmoRotate)] = { Key::E, false, false, false };
		m_Shortcuts[static_cast<size_t>(EditorShortcutAction::GizmoScale)] = { Key::R, false, false, false };
		m_Shortcuts[static_cast<size_t>(EditorShortcutAction::OpenSettings)] = { Key::Comma, true, false, false };
		m_Shortcuts[static_cast<size_t>(EditorShortcutAction::OpenCommandPalette)] = { Key::P, true, true, false };
		m_ShortcutsInitialized = true;
	}

	bool UISettings::ShortcutMatches(EditorShortcutAction action, KeyCode keyIn, bool ctrl, bool shift, bool alt) const
	{
		const size_t index = static_cast<size_t>(action);
		if (index >= m_Shortcuts.size() || HasShortcutConflict(index))
			return false;
		const ShortcutBinding& binding = m_Shortcuts[index];
		return binding.m_Key == keyIn && binding.m_Ctrl == ctrl && binding.m_Shift == shift && binding.m_Alt == alt;
	}

	bool UISettings::HasShortcutConflict(EditorShortcutAction action) const
	{
		return HasShortcutConflict(static_cast<size_t>(action));
	}

	bool UISettings::HasShortcutConflict(size_t index) const
	{
		for (size_t i = 0; i < m_Shortcuts.size(); ++i)
			if (i != index && SameBinding(m_Shortcuts[index], m_Shortcuts[i]))
				return true;
		return false;
	}

	std::string UISettings::ShortcutLabel(const ShortcutBinding& binding) const
	{
		std::string label;
		if (binding.m_Ctrl)
			label += "Ctrl+";
		if (binding.m_Shift)
			label += "Shift+";
		if (binding.m_Alt)
			label += "Alt+";
		label += Key::ToString(binding.m_Key);
		return label;
	}

	std::string UISettings::GetShortcutLabel(EditorShortcutAction action) const
	{
		const size_t index = static_cast<size_t>(action);
		if (index >= m_Shortcuts.size())
			return {};
		return ShortcutLabel(m_Shortcuts[index]);
	}

	ShortcutBinding UISettings::GetShortcutBinding(EditorShortcutAction action) const
	{
		const size_t index = static_cast<size_t>(action);
		if (index >= m_Shortcuts.size())
			return {};
		return m_Shortcuts[index];
	}

	void UISettings::SetShortcutBinding(EditorShortcutAction action, const ShortcutBinding& binding)
	{
		const size_t index = static_cast<size_t>(action);
		if (index >= m_Shortcuts.size())
			return;
		m_Shortcuts[index] = binding;
		m_ShortcutsInitialized = true;
		MarkDirty();
	}

	void UISettings::SetShowPhysicsColliders(bool value)
	{
		m_ShowPhysicsColliders = value;
		MarkDirty();
	}

	void UISettings::SetStepFrame(int value)
	{
		m_StepFrame = value < 1 ? 1 : value;
		MarkDirty();
	}

	void UISettings::SetSnapValues(uint32_t idx, const glm::vec3& value)
	{
		if (idx >= 3)
			return;
		m_SnapValues[idx] = value;
		MarkDirty();
	}

	void UISettings::SetTheme(EditorTheme theme)
	{
		m_Theme = theme;
		ApplyEditorTheme(theme);
		MarkDirty();
	}

	void UISettings::SetAssistantSettings(const Assistant::Settings& settings)
	{
		m_AssistantSettings = settings;
		MarkDirty();
	}

	bool UISettings::ConsumeDirty()
	{
		bool dirty = m_Dirty;
		m_Dirty = false;
		return dirty;
	}

	const char* UISettings::GetActionDisplayName(EditorShortcutAction action)
	{
		switch (action)
		{
		case EditorShortcutAction::NewScene: return "New Scene";
		case EditorShortcutAction::OpenProject: return "Open Project";
		case EditorShortcutAction::SaveScene: return "Save Scene";
		case EditorShortcutAction::SaveSceneAs: return "Save Scene As";
		case EditorShortcutAction::SaveProject: return "Save Project";
		case EditorShortcutAction::CloseScene: return "Close Scene";
		case EditorShortcutAction::ReloadScripts: return "Reload Scripts";
		case EditorShortcutAction::DuplicateEntity: return "Duplicate Entity";
		case EditorShortcutAction::DeleteEntity: return "Delete Entity";
		case EditorShortcutAction::Undo: return "Undo";
		case EditorShortcutAction::Redo: return "Redo";
		case EditorShortcutAction::SelectAll: return "Select All";
		case EditorShortcutAction::Copy: return "Copy";
		case EditorShortcutAction::Paste: return "Paste";
		case EditorShortcutAction::Cut: return "Cut";
		case EditorShortcutAction::Play: return "Play";
		case EditorShortcutAction::Simulate: return "Simulate";
		case EditorShortcutAction::Stop: return "Stop";
		case EditorShortcutAction::Pause: return "Pause";
		case EditorShortcutAction::GizmoNone: return "Select Tool";
		case EditorShortcutAction::GizmoTranslate: return "Move Tool";
		case EditorShortcutAction::GizmoRotate: return "Rotate Tool";
		case EditorShortcutAction::GizmoScale: return "Scale Tool";
		case EditorShortcutAction::OpenSettings: return "Open Settings";
		case EditorShortcutAction::OpenCommandPalette: return "Command Palette";
		default: return "Unknown";
		}
	}

	const char* UISettings::GetActionStorageKey(EditorShortcutAction action)
	{
		switch (action)
		{
		case EditorShortcutAction::NewScene: return "new_scene";
		case EditorShortcutAction::OpenProject: return "open_project";
		case EditorShortcutAction::SaveScene: return "SaveScene";
		case EditorShortcutAction::SaveSceneAs: return "save_scene_as";
		case EditorShortcutAction::SaveProject: return "save_project";
		case EditorShortcutAction::CloseScene: return "close_scene";
		case EditorShortcutAction::ReloadScripts: return "reload_scripts";
		case EditorShortcutAction::DuplicateEntity: return "duplicate_entity";
		case EditorShortcutAction::DeleteEntity: return "delete_entity";
		case EditorShortcutAction::Undo: return "undo";
		case EditorShortcutAction::Redo: return "redo";
		case EditorShortcutAction::SelectAll: return "select_all";
		case EditorShortcutAction::Copy: return "copy";
		case EditorShortcutAction::Paste: return "paste";
		case EditorShortcutAction::Cut: return "cut";
		case EditorShortcutAction::Play: return "play";
		case EditorShortcutAction::Simulate: return "simulate";
		case EditorShortcutAction::Stop: return "stop";
		case EditorShortcutAction::Pause: return "pause";
		case EditorShortcutAction::GizmoNone: return "gizmo_none";
		case EditorShortcutAction::GizmoTranslate: return "gizmo_translate";
		case EditorShortcutAction::GizmoRotate: return "gizmo_rotate";
		case EditorShortcutAction::GizmoScale: return "gizmo_scale";
		case EditorShortcutAction::OpenSettings: return "open_settings";
		case EditorShortcutAction::OpenCommandPalette: return "open_command_palette";
		default: return "unknown";
		}
	}

	const char* UISettings::GetThemeName(EditorTheme theme)
	{
		switch (theme)
		{
		case EditorTheme::WhipDark: return "Carbon";
		case EditorTheme::Graphite: return "Graphite";
		case EditorTheme::Ember: return "Ember";
		case EditorTheme::Moss: return "Moss";
		case EditorTheme::Light: return "Porcelain";
		default: return "Carbon";
		}
	}

	const char* UISettings::GetThemeDescription(EditorTheme theme)
	{
		switch (theme)
		{
		case EditorTheme::WhipDark: return "Dark graphite shell with cool steel highlights";
		case EditorTheme::Graphite: return "Neutral studio graphite with brass accents";
		case EditorTheme::Ember: return "Deep brown-black surfaces with copper focus";
		case EditorTheme::Moss: return "Soft green-black panels with muted moss accents";
		case EditorTheme::Light: return "Warm light workspace for bright rooms";
		default: return "Warm charcoal, quiet amber highlights";
		}
	}

	const char* UISettings::GetActionCategory(EditorShortcutAction action)
	{
		switch (action)
		{
		case EditorShortcutAction::NewScene:
		case EditorShortcutAction::OpenProject:
		case EditorShortcutAction::SaveScene:
		case EditorShortcutAction::SaveSceneAs:
		case EditorShortcutAction::SaveProject:
		case EditorShortcutAction::CloseScene:
			return "File";
		case EditorShortcutAction::Undo:
		case EditorShortcutAction::Redo:
		case EditorShortcutAction::SelectAll:
		case EditorShortcutAction::Copy:
		case EditorShortcutAction::Paste:
		case EditorShortcutAction::Cut:
		case EditorShortcutAction::DuplicateEntity:
		case EditorShortcutAction::DeleteEntity:
			return "Edit";
		case EditorShortcutAction::Play:
		case EditorShortcutAction::Simulate:
		case EditorShortcutAction::Stop:
		case EditorShortcutAction::Pause:
			return "Run";
		case EditorShortcutAction::GizmoNone:
		case EditorShortcutAction::GizmoTranslate:
		case EditorShortcutAction::GizmoRotate:
		case EditorShortcutAction::GizmoScale:
			return "Tools";
		case EditorShortcutAction::ReloadScripts:
			return "Script";
		case EditorShortcutAction::OpenSettings:
		case EditorShortcutAction::OpenCommandPalette:
			return "Window";
		default:
			return "General";
		}
	}

	void UISettings::DrawGeneralSettings()
	{
		DrawSettingsHeading("Viewport");
		if (ImGui::Checkbox("Show physics colliders", &m_ShowPhysicsColliders))
			MarkDirty();
		ImGui::SetNextItemWidth(140.0f);
		if (ImGui::InputInt("Step Frame", &m_StepFrame))
			MarkDirty();
		if (m_StepFrame < 1)
			m_StepFrame = 1;

		DrawSettingsHeading("Gizmo Snap");
		UI::DrawVec3Control("Translation", m_SnapValues[0]);
		if (ImGui::IsItemDeactivatedAfterEdit())
			MarkDirty();
		UI::DrawVec3Control("Rotation", m_SnapValues[1]);
		if (ImGui::IsItemDeactivatedAfterEdit())
			MarkDirty();
		UI::DrawVec3Control("Scale", m_SnapValues[2]);
		if (ImGui::IsItemDeactivatedAfterEdit())
			MarkDirty();
	}

	void UISettings::DrawAppearanceSettings()
	{
		DrawSettingsHeading("Appearance");
		const char* currentTheme = GetThemeName(m_Theme);
		if (ImGui::BeginCombo("Theme", currentTheme))
		{
			for (EditorTheme theme : { EditorTheme::WhipDark, EditorTheme::Graphite, EditorTheme::Ember, EditorTheme::Moss, EditorTheme::Light })
			{
				const bool selected = m_Theme == theme;
				if (ImGui::Selectable(GetThemeName(theme), selected))
					SetTheme(theme);
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("%s", GetThemeDescription(theme));
				if (selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		ImGui::TextDisabled("%s", GetThemeDescription(m_Theme));
	}

	void UISettings::DrawAssistantSettings()
	{
		DrawSettingsHeading("Whip Assistant");
		if (ImGui::Checkbox("Enable assistant", &m_AssistantSettings.m_Enabled))
			MarkDirty();
		if (ImGui::Checkbox("Allow online OpenAI responses", &m_AssistantSettings.m_UseOnlineResponses))
			MarkDirty();
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("When disabled, Whip Assistant only creates local editor proposals.");

		ImGui::SetNextItemWidth(220.0f);
		if (ImGui::InputText("Model", &m_AssistantSettings.m_Model))
			MarkDirty();
		ImGui::SetNextItemWidth(360.0f);
		if (ImGui::InputText("API Key", &m_AssistantSettings.m_ApiKey, ImGuiInputTextFlags_Password))
			MarkDirty();
		ImGui::TextDisabled("The key is kept in local editor preferences. Scene edits still require Apply.");

		if (ImGui::Checkbox("Send scene selection context", &m_AssistantSettings.m_SendSceneContext))
			MarkDirty();
		if (ImGui::Checkbox("Send recent console context", &m_AssistantSettings.m_SendConsoleContext))
			MarkDirty();
	}

	void UISettings::DrawShortcutSettings()
	{
		DrawSettingsHeading("Shortcut Map");
		if (m_ShortcutManager)
		{
			m_ShortcutManager->DrawSettings();
			return;
		}

		if (ImGui::Button("Reset Defaults", ImVec2(128.0f, 0.0f)))
			ResetShortcutsToDefault();

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

			for (size_t i = 0; i < m_Shortcuts.size(); ++i)
			{
				ShortcutBinding& binding = m_Shortcuts[i];
				const bool conflict = HasShortcutConflict(i);
				ImGui::PushID(static_cast<int>(i));
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(GetActionDisplayName(static_cast<EditorShortcutAction>(i)));
				ImGui::TableNextColumn();
				ImGui::TextDisabled("%s", GetActionCategory(static_cast<EditorShortcutAction>(i)));
				ImGui::TableNextColumn();
				ImGui::SetNextItemWidth(-1.0f);
				if (ImGui::BeginCombo("##Key", Key::ToString(binding.m_Key)))
				{
					for (KeyCode candidate : EditableKeys)
					{
						const bool selected = binding.m_Key == candidate;
						if (ImGui::Selectable(Key::ToString(candidate), selected))
						{
							binding.m_Key = candidate;
							MarkDirty();
						}
						if (selected)
							ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}
				ImGui::TableNextColumn();
				if (ImGui::Checkbox("##Ctrl", &binding.m_Ctrl))
					MarkDirty();
				ImGui::TableNextColumn();
				if (ImGui::Checkbox("##Shift", &binding.m_Shift))
					MarkDirty();
				ImGui::TableNextColumn();
				if (ImGui::Checkbox("##Alt", &binding.m_Alt))
					MarkDirty();
				ImGui::TableNextColumn();
				if (conflict)
					ImGui::TextColored(ImVec4(0.95f, 0.50f, 0.34f, 1.0f), "Conflict");
				else
					ImGui::TextDisabled("%s", ShortcutLabel(binding).c_str());
				ImGui::PopID();
			}
			ImGui::EndTable();
		}
	}

	void UISettings::DrawContent()
	{
		if (!m_ShortcutsInitialized)
			ResetShortcutsToDefault();

		DrawGeneralSettings();
		ImGui::Spacing();
		DrawAppearanceSettings();
		ImGui::Spacing();
		DrawAssistantSettings();
		ImGui::Spacing();
		DrawShortcutSettings();
	}

	void UISettings::MarkDirty()
	{
		m_Dirty = true;
	}

	void UISettings::OnImGuiRender()
	{
		if (!m_ShortcutsInitialized)
			ResetShortcutsToDefault();

		if (m_Open)
		{
			ImGui::SetNextWindowSize(ImVec2(740.0f, 520.0f), ImGuiCond_FirstUseEver);
			ImGui::Begin("Settings", &m_Open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking);
			DrawContent();
			ImGui::End();
		}
	}
}

_WHIP_END
