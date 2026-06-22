#pragma once

#include <Whip.h>
#include <Whip-Editor/UI/UISettings.h>

#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <yaml-cpp/yaml.h>

#include "EditorManagerBase.h"

_WHIP_START

enum class EditorShortcutScope : uint8_t
{
	Global = 0,
	Viewport,
	SceneHierarchy,
	ContentBrowser,
	AssetEditor,
	AnimationEditor,
	Console,
	Assistant,
	Statistics,
	ProjectHub
};

struct EditorShortcutOptions
{
	bool m_AllowWhenActiveWidget = false;
	bool m_AllowWhenTextInput = false;
	bool m_HiddenFromSettings = false;
	bool m_HiddenFromCommandPalette = false;
};

struct EditorShortcut
{
	std::string m_Id;
	std::string m_DisplayName;
	std::string m_Category;
	EditorShortcutScope m_Scope = EditorShortcutScope::Global;
	UI::ShortcutBinding m_DefaultBinding{};
	UI::ShortcutBinding m_Binding{};
	std::function<bool()> m_Callback;
	std::function<bool()> m_IsAvailable;
	std::function<bool()> m_IsContextActive;
	EditorShortcutOptions m_Options{};
};

class EditorShortcutManager : public EditorManagerBase // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	explicit EditorShortcutManager(EditorLayer* boundedLayer = nullptr);
	~EditorShortcutManager() override;

	void Clear();
	void Add(EditorShortcut shortcut);
	void Add(
		EditorShortcutScope scope,
		std::string id,
		std::string displayName,
		std::string category,
		const UI::ShortcutBinding& defaultBinding,
		std::function<bool()> callback,
		std::function<bool()> isAvailable = {},
		std::function<bool()> isContextActive = {},
		const EditorShortcutOptions& options = {});

	bool HandleKeyPressed(KeyPressedEvent& event, bool hasActiveWidget);
	bool HandleImGuiShortcuts(bool hasActiveWidget);
	void DrawSettings();

	bool ConsumeDirty();
	void ResetToDefaults();
	void ResetScopeToDefaults(EditorShortcutScope scope);
	void LoadBindings(const YAML::Node& shortcuts);
	void SaveBindings(YAML::Emitter& out) const;
	void SyncLegacyGlobalBindings(const UI::UISettings& settings);

	const std::vector<EditorShortcut>& GetShortcuts() const { return m_Shortcuts; }
	bool ExecuteShortcut(std::string_view id, bool hasActiveWidget = false, bool ignoreTextInput = false, bool ignoreContext = false) const;
	bool IsShortcutAvailable(std::string_view id, bool requireActiveContext = true) const;
	UI::ShortcutBinding GetBinding(std::string_view id, const UI::ShortcutBinding& fallback = {}) const;
	std::string GetShortcutLabel(std::string_view id) const;
	bool HasConflict(std::string_view id) const;
	std::string GetConflictDescription(std::string_view id) const;
	bool IsCapturingBinding() const { return !m_CapturingShortcutId.empty(); }
	void CancelBindingCapture();
	void DrawShortcutTooltip(std::string_view id, const char* description = nullptr) const;

	static const char* GetScopeName(EditorShortcutScope scope);
	static std::string ShortcutLabel(const UI::ShortcutBinding& binding);

private:
	EditorShortcut* FindShortcut(std::string_view id);
	const EditorShortcut* FindShortcut(std::string_view id) const;
	bool IsShortcutActive(const EditorShortcut& shortcut) const;
	bool HandleShortcut(KeyCode key, bool ctrl, bool shift, bool alt, bool hasActiveWidget) const;
	bool Matches(const EditorShortcut& shortcut, KeyCode key, bool ctrl, bool shift, bool alt) const;
	bool Execute(const EditorShortcut& shortcut, bool hasActiveWidget, bool ignoreTextInput = false, bool ignoreContext = false) const;
	bool HasConflict(const EditorShortcut& shortcut) const;
	std::string GetConflictDescription(const EditorShortcut& shortcut) const;
	void MarkDirty();

	std::vector<EditorShortcut> m_Shortcuts;
	std::unordered_map<std::string, UI::ShortcutBinding> m_PendingBindings;
	std::string m_CapturingShortcutId;
	char m_SearchBuffer[128]{ 0 };
	int m_SettingsScopeFilter = -1;
	bool m_Dirty = false;
};

using ShortcutManager = EditorShortcutManager;

_WHIP_END
