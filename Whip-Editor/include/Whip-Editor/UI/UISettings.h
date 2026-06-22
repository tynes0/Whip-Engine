#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/Log.h>
#include <Whip/Core/KeyCodes.h>
#include <Whip-Editor/Assistant/WhipAssistant.h>

#include <array>
#include <string>

#include <glm/vec3.hpp>

_WHIP_START

class EditorShortcutManager;

namespace UI
{
	enum class EditorShortcutAction : uint8_t
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

	struct ShortcutBinding
	{
		KeyCode m_Key = 0;
		bool m_Ctrl = false;
		bool m_Shift = false;
		bool m_Alt = false;
	};

	enum class EditorTheme : uint8_t
	{
		WhipDark = 0,
		Graphite,
		Ember,
		Moss,
		Light
	};

	void ApplyEditorTheme(EditorTheme theme);

	class UISettings
	{
	public:
		static constexpr size_t ActionCount = static_cast<size_t>(EditorShortcutAction::Count);

		UISettings();

		bool GetShowPhysicsColliders() const { return m_ShowPhysicsColliders; }
		const glm::vec3& GetSnapValues(uint32_t idx) const { return m_SnapValues[idx < 3 ? idx : 0]; }
		int GetStepFrame() const { return m_StepFrame; }
		bool ShortcutMatches(EditorShortcutAction action, KeyCode key, bool ctrl, bool shift, bool alt) const;
		bool HasShortcutConflict(EditorShortcutAction action) const;
		std::string GetShortcutLabel(EditorShortcutAction action) const;
		ShortcutBinding GetShortcutBinding(EditorShortcutAction action) const;
		void SetShortcutBinding(EditorShortcutAction action, const ShortcutBinding& binding);
		void SetShowPhysicsColliders(bool value);
		void SetStepFrame(int value);
		void SetSnapValues(uint32_t idx, const glm::vec3& value);
		EditorTheme GetTheme() const { return m_Theme; }
		void SetTheme(EditorTheme theme);
		const Assistant::Settings& GetAssistantSettings() const { return m_AssistantSettings; }
		void SetAssistantSettings(const Assistant::Settings& settings);
		bool ConsumeDirty();
		static const char* GetActionDisplayName(EditorShortcutAction action);
		static const char* GetActionCategory(EditorShortcutAction action);
		static const char* GetActionStorageKey(EditorShortcutAction action);
		static const char* GetThemeName(EditorTheme theme);
		static const char* GetThemeDescription(EditorTheme theme);

		void SetShortcutManager(EditorShortcutManager* shortcutManager) { m_ShortcutManager = shortcutManager; }
		void OpenWindow() { m_Open = true; }

		void DrawContent();
		void OnImGuiRender();
	private:
		void DrawGeneralSettings();
		void DrawAppearanceSettings();
		void DrawAssistantSettings();
		void DrawShortcutSettings();
		void ResetShortcutsToDefault();
		bool HasShortcutConflict(size_t index) const;
		std::string ShortcutLabel(const ShortcutBinding& binding) const;
		void MarkDirty();

		bool m_ShowPhysicsColliders = false;
		glm::vec3 m_SnapValues[3] = { {0.5f, 0.5f, 0.5f}, {45.0f, 45.0f, 45.0f}, {0.5f, 0.5f, 0.5f} };
		int m_StepFrame = 1;
		EditorTheme m_Theme = EditorTheme::WhipDark;
		Assistant::Settings m_AssistantSettings;
		std::array<ShortcutBinding, ActionCount> m_Shortcuts = {};
		bool m_ShortcutsInitialized = false;
		bool m_Dirty = false;
		EditorShortcutManager* m_ShortcutManager = nullptr;

		bool m_Open = false;
	};
}

_WHIP_END
