#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/Log.h>
#include <Whip/Core/KeyCodes.h>

#include <array>
#include <string>

#include <glm/vec3.hpp>

_WHIP_START

namespace UI
{
	enum class editor_shortcut_action : uint8_t
	{
		NewScene = 0,
		OpenProject,
		SaveScene,
		SaveSceneAs,
		SaveProject,
		CloseScene,
		ReloadScripts,
		DuplicateEntity,
		DeleteEntity,
		Undo,
		Redo,
		SelectAll,
		Copy,
		Paste,
		Cut,
		Play,
		Simulate,
		Stop,
		Pause,
		GizmoNone,
		GizmoTranslate,
		GizmoRotate,
		GizmoScale,
		OpenSettings,
		OpenCommandPalette,
		Count
	};

	struct shortcut_binding
	{
		key_code key = 0;
		bool ctrl = false;
		bool shift = false;
		bool alt = false;
	};

	enum class editor_theme : uint8_t
	{
		whip_dark = 0,
		graphite,
		ember,
		moss,
		light
	};

	void apply_editor_theme(editor_theme theme);

	class UI_settings
	{
	public:
		static constexpr size_t action_count = static_cast<size_t>(editor_shortcut_action::Count);

		UI_settings();

		bool get_show_physics_colliders() const { return m_show_physics_colliders; }
		const glm::vec3& get_snap_values(uint32_t idx) const { return m_snap_values[idx < 3 ? idx : 0]; }
		int get_step_frame() const { return m_step_frame; }
		bool shortcut_matches(editor_shortcut_action action, key_code key, bool ctrl, bool shift, bool alt) const;
		bool has_shortcut_conflict(editor_shortcut_action action) const;
		std::string get_shortcut_label(editor_shortcut_action action) const;
		shortcut_binding get_shortcut_binding(editor_shortcut_action action) const;
		void set_shortcut_binding(editor_shortcut_action action, const shortcut_binding& binding);
		void set_show_physics_colliders(bool value);
		void set_step_frame(int value);
		void set_snap_values(uint32_t idx, const glm::vec3& value);
		editor_theme get_theme() const { return m_theme; }
		void set_theme(editor_theme theme);
		bool consume_dirty();
		static const char* get_action_display_name(editor_shortcut_action action);
		static const char* get_action_category(editor_shortcut_action action);
		static const char* get_action_storage_key(editor_shortcut_action action);
		static const char* get_theme_name(editor_theme theme);
		static const char* get_theme_description(editor_theme theme);

		void open_window() { m_open = true; }

		void draw_content();
		void on_imgui_render();
	private:
		void draw_general_settings();
		void draw_appearance_settings();
		void draw_shortcut_settings();
		void reset_shortcuts_to_default();
		bool has_shortcut_conflict(size_t index) const;
		std::string shortcut_label(const shortcut_binding& binding) const;
		void mark_dirty();

		bool m_show_physics_colliders = false;
		glm::vec3 m_snap_values[3] = { {0.5f, 0.5f, 0.5f}, {45.0f, 45.0f, 45.0f}, {0.5f, 0.5f, 0.5f} };
		int m_step_frame = 1;
		editor_theme m_theme = editor_theme::whip_dark;
		std::array<shortcut_binding, action_count> m_shortcuts = {};
		bool m_shortcuts_initialized = false;
		bool m_dirty = false;

		bool m_open = false;
	};
}

_WHIP_END
