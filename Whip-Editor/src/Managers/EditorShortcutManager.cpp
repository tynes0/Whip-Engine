#include <WhipPch.h>

#include <Whip-Editor/Managers/EditorShortcutManager.h>

#include <Whip-Editor/EditorLayer.h>

#include <algorithm>
#include <cctype>

#include <imgui.h>

_WHIP_START

namespace
{
	bool SameBinding(const UI::ShortcutBinding& left, const UI::ShortcutBinding& right)
	{
		return left.m_Key != 0 &&
			left.m_Key == right.m_Key &&
			left.m_Ctrl == right.m_Ctrl &&
			left.m_Shift == right.m_Shift &&
			left.m_Alt == right.m_Alt;
	}

	bool IsModifierKey(KeyCode key)
	{
		return key == Key::LeftControl || key == Key::RightControl ||
			key == Key::LeftShift || key == Key::RightShift ||
			key == Key::LeftAlt || key == Key::RightAlt;
	}

	bool IsTextEditingBinding(const UI::ShortcutBinding& binding)
	{
		if (!binding.m_Ctrl || binding.m_Alt)
			return false;

		switch (binding.m_Key)
		{
		case Key::A:
		case Key::C:
		case Key::V:
		case Key::X:
		case Key::Y:
		case Key::Z:
			return true;
		default:
			return false;
		}
	}

	ImGuiKey ToImGuiKey(KeyCode key)
	{
		switch (key)
		{
		case Key::Space: return ImGuiKey_Space;
		case Key::Apostrophe: return ImGuiKey_Apostrophe;
		case Key::Comma: return ImGuiKey_Comma;
		case Key::Minus: return ImGuiKey_Minus;
		case Key::Period: return ImGuiKey_Period;
		case Key::Slash: return ImGuiKey_Slash;
		case Key::D0: return ImGuiKey_0;
		case Key::D1: return ImGuiKey_1;
		case Key::D2: return ImGuiKey_2;
		case Key::D3: return ImGuiKey_3;
		case Key::D4: return ImGuiKey_4;
		case Key::D5: return ImGuiKey_5;
		case Key::D6: return ImGuiKey_6;
		case Key::D7: return ImGuiKey_7;
		case Key::D8: return ImGuiKey_8;
		case Key::D9: return ImGuiKey_9;
		case Key::Semicolon: return ImGuiKey_Semicolon;
		case Key::Equal: return ImGuiKey_Equal;
		case Key::A: return ImGuiKey_A;
		case Key::B: return ImGuiKey_B;
		case Key::C: return ImGuiKey_C;
		case Key::D: return ImGuiKey_D;
		case Key::E: return ImGuiKey_E;
		case Key::F: return ImGuiKey_F;
		case Key::G: return ImGuiKey_G;
		case Key::H: return ImGuiKey_H;
		case Key::I: return ImGuiKey_I;
		case Key::J: return ImGuiKey_J;
		case Key::K: return ImGuiKey_K;
		case Key::L: return ImGuiKey_L;
		case Key::M: return ImGuiKey_M;
		case Key::N: return ImGuiKey_N;
		case Key::O: return ImGuiKey_O;
		case Key::P: return ImGuiKey_P;
		case Key::Q: return ImGuiKey_Q;
		case Key::R: return ImGuiKey_R;
		case Key::S: return ImGuiKey_S;
		case Key::T: return ImGuiKey_T;
		case Key::U: return ImGuiKey_U;
		case Key::V: return ImGuiKey_V;
		case Key::W: return ImGuiKey_W;
		case Key::X: return ImGuiKey_X;
		case Key::Y: return ImGuiKey_Y;
		case Key::Z: return ImGuiKey_Z;
		case Key::LeftBracket: return ImGuiKey_LeftBracket;
		case Key::Backslash: return ImGuiKey_Backslash;
		case Key::RightBracket: return ImGuiKey_RightBracket;
		case Key::GraveAccent: return ImGuiKey_GraveAccent;
		case Key::Escape: return ImGuiKey_Escape;
		case Key::Enter: return ImGuiKey_Enter;
		case Key::Tab: return ImGuiKey_Tab;
		case Key::Backspace: return ImGuiKey_Backspace;
		case Key::Insert: return ImGuiKey_Insert;
		case Key::Delete: return ImGuiKey_Delete;
		case Key::Right: return ImGuiKey_RightArrow;
		case Key::Left: return ImGuiKey_LeftArrow;
		case Key::Down: return ImGuiKey_DownArrow;
		case Key::Up: return ImGuiKey_UpArrow;
		case Key::PageUp: return ImGuiKey_PageUp;
		case Key::PageDown: return ImGuiKey_PageDown;
		case Key::Home: return ImGuiKey_Home;
		case Key::End: return ImGuiKey_End;
		case Key::F1: return ImGuiKey_F1;
		case Key::F2: return ImGuiKey_F2;
		case Key::F3: return ImGuiKey_F3;
		case Key::F4: return ImGuiKey_F4;
		case Key::F5: return ImGuiKey_F5;
		case Key::F6: return ImGuiKey_F6;
		case Key::F7: return ImGuiKey_F7;
		case Key::F8: return ImGuiKey_F8;
		case Key::F9: return ImGuiKey_F9;
		case Key::F10: return ImGuiKey_F10;
		case Key::F11: return ImGuiKey_F11;
		case Key::F12: return ImGuiKey_F12;
		case Key::KP0: return ImGuiKey_Keypad0;
		case Key::KP1: return ImGuiKey_Keypad1;
		case Key::KP2: return ImGuiKey_Keypad2;
		case Key::KP3: return ImGuiKey_Keypad3;
		case Key::KP4: return ImGuiKey_Keypad4;
		case Key::KP5: return ImGuiKey_Keypad5;
		case Key::KP6: return ImGuiKey_Keypad6;
		case Key::KP7: return ImGuiKey_Keypad7;
		case Key::KP8: return ImGuiKey_Keypad8;
		case Key::KP9: return ImGuiKey_Keypad9;
		case Key::KPDecimal: return ImGuiKey_KeypadDecimal;
		case Key::KPDivide: return ImGuiKey_KeypadDivide;
		case Key::KPMultiply: return ImGuiKey_KeypadMultiply;
		case Key::KPSubtract: return ImGuiKey_KeypadSubtract;
		case Key::KPAdd: return ImGuiKey_KeypadAdd;
		case Key::KPEnter: return ImGuiKey_KeypadEnter;
		case Key::KPEqual: return ImGuiKey_KeypadEqual;
		default: return ImGuiKey_None;
		}
	}

	std::string LowerCopy(std::string value)
	{
		std::ranges::transform(value, value.begin(),
			[](unsigned char character) { return static_cast<char>(std::tolower(character)); });
		return value;
	}

	bool TextMatchesFilter(const EditorShortcut& shortcut, const char* filter)
	{
		if (!filter || filter[0] == '\0')
			return true;

		const std::string needle = LowerCopy(filter);
		return LowerCopy(shortcut.m_DisplayName).find(needle) != std::string::npos ||
			LowerCopy(shortcut.m_Category).find(needle) != std::string::npos ||
			LowerCopy(shortcut.m_Id).find(needle) != std::string::npos ||
			LowerCopy(EditorShortcutManager::GetScopeName(shortcut.m_Scope)).find(needle) != std::string::npos;
	}

	void WriteBinding(YAML::Emitter& out, const UI::ShortcutBinding& binding)
	{
		out << YAML::BeginMap;
		out << YAML::Key << "key" << YAML::Value << binding.m_Key;
		out << YAML::Key << "ctrl" << YAML::Value << binding.m_Ctrl;
		out << YAML::Key << "shift" << YAML::Value << binding.m_Shift;
		out << YAML::Key << "alt" << YAML::Value << binding.m_Alt;
		out << YAML::EndMap;
	}
}

EditorShortcutManager::EditorShortcutManager(EditorLayer* boundedLayer)
	: EditorManagerBase(boundedLayer)
{
}

EditorShortcutManager::~EditorShortcutManager() = default;

void EditorShortcutManager::Clear()
{
	m_Shortcuts.clear();
}

void EditorShortcutManager::Add(EditorShortcut shortcut)
{
	if (shortcut.m_Id.empty())
		return;

	shortcut.m_Binding = shortcut.m_Binding.m_Key == 0 ? shortcut.m_DefaultBinding : shortcut.m_Binding;
	if (auto pendingIt = m_PendingBindings.find(shortcut.m_Id); pendingIt != m_PendingBindings.end())
	{
		shortcut.m_Binding = pendingIt->second;
		m_PendingBindings.erase(pendingIt);
	}

	if (EditorShortcut* existing = FindShortcut(shortcut.m_Id))
	{
		const UI::ShortcutBinding currentBinding = existing->m_Binding;
		*existing = std::move(shortcut);
		existing->m_Binding = currentBinding;
		return;
	}

	m_Shortcuts.push_back(std::move(shortcut));
}

void EditorShortcutManager::Add(
	EditorShortcutScope scope,
	std::string id,
	std::string displayName,
	std::string category,
	const UI::ShortcutBinding& defaultBinding,
	std::function<bool()> callback,
	std::function<bool()> isAvailable,
	std::function<bool()> isContextActive,
	const EditorShortcutOptions& options)
{
	EditorShortcut shortcut;
	shortcut.m_Id = std::move(id);
	shortcut.m_DisplayName = std::move(displayName);
	shortcut.m_Category = std::move(category);
	shortcut.m_Scope = scope;
	shortcut.m_DefaultBinding = defaultBinding;
	shortcut.m_Binding = defaultBinding;
	shortcut.m_Callback = std::move(callback);
	shortcut.m_IsAvailable = std::move(isAvailable);
	shortcut.m_IsContextActive = std::move(isContextActive);
	shortcut.m_Options = options;
	Add(std::move(shortcut));
}

bool EditorShortcutManager::HandleKeyPressed(KeyPressedEvent& event, bool hasActiveWidget)
{
	if (event.GetRepeatCount() > 0)
		return IsCapturingBinding();

	const bool control = Input::IsKeyDown(Key::LeftControl) || Input::IsKeyDown(Key::RightControl);
	const bool shift = Input::IsKeyDown(Key::LeftShift) || Input::IsKeyDown(Key::RightShift);
	const bool alt = Input::IsKeyDown(Key::LeftAlt) || Input::IsKeyDown(Key::RightAlt);
	const KeyCode key = event.GetKeyCode();

	if (IsCapturingBinding())
	{
		if (!IsModifierKey(key))
		{
			if (EditorShortcut* shortcut = FindShortcut(m_CapturingShortcutId))
			{
				shortcut->m_Binding = { key, control, shift, alt };
				MarkDirty();
			}
			m_CapturingShortcutId.clear();
		}
		return true;
	}

	return HandleShortcut(key, control, shift, alt, hasActiveWidget);
}

bool EditorShortcutManager::HandleImGuiShortcuts(bool hasActiveWidget)
{
	if (IsCapturingBinding())
		return false;

	const ImGuiIO& io = ImGui::GetIO();
	const bool control = io.KeyCtrl;
	const bool shift = io.KeyShift;
	const bool alt = io.KeyAlt;

	for (const EditorShortcut& shortcut : m_Shortcuts)
	{
		if (shortcut.m_Binding.m_Key == 0 ||
			shortcut.m_Binding.m_Ctrl != control ||
			shortcut.m_Binding.m_Shift != shift ||
			shortcut.m_Binding.m_Alt != alt)
			continue;

		const ImGuiKey imguiKey = ToImGuiKey(shortcut.m_Binding.m_Key);
		if (imguiKey == ImGuiKey_None || !ImGui::IsKeyPressed(imguiKey, false))
			continue;

		if (HandleShortcut(shortcut.m_Binding.m_Key, control, shift, alt, hasActiveWidget))
			return true;
	}

	return false;
}

void EditorShortcutManager::DrawSettings()
{
	ImGui::TextDisabled("Context aware shortcuts. Same keys can safely exist in different panels.");
	ImGui::Spacing();
	ImGui::SetNextItemWidth(260.0f);
	ImGui::InputTextWithHint("##ShortcutSearch", "Search shortcuts...", m_SearchBuffer, sizeof(m_SearchBuffer));
	ImGui::SameLine();
	ImGui::SetNextItemWidth(170.0f);
	if (ImGui::BeginCombo("##ShortcutScopeFilter", m_SettingsScopeFilter < 0 ? "All Scopes" : GetScopeName(static_cast<EditorShortcutScope>(m_SettingsScopeFilter))))
	{
		if (ImGui::Selectable("All Scopes", m_SettingsScopeFilter < 0))
			m_SettingsScopeFilter = -1;
		for (EditorShortcutScope scope : {
			EditorShortcutScope::Global,
			EditorShortcutScope::Viewport,
			EditorShortcutScope::SceneHierarchy,
			EditorShortcutScope::ContentBrowser,
			EditorShortcutScope::AssetEditor,
			EditorShortcutScope::AnimationEditor,
			EditorShortcutScope::Console,
			EditorShortcutScope::Assistant,
			EditorShortcutScope::Statistics,
			EditorShortcutScope::ProjectHub })
		{
			const int scopeIndex = static_cast<int>(scope);
			if (ImGui::Selectable(GetScopeName(scope), m_SettingsScopeFilter == scopeIndex))
				m_SettingsScopeFilter = scopeIndex;
		}
		ImGui::EndCombo();
	}
	ImGui::SameLine();
	if (ImGui::Button("Reset Defaults", ImVec2(128.0f, 0.0f)))
		ResetToDefaults();
	if (m_SettingsScopeFilter >= 0)
	{
		ImGui::SameLine();
		if (ImGui::Button("Reset Scope", ImVec2(118.0f, 0.0f)))
			ResetScopeToDefaults(static_cast<EditorShortcutScope>(m_SettingsScopeFilter));
	}

	constexpr EditorShortcutScope Scopes[] =
	{
		EditorShortcutScope::Global,
		EditorShortcutScope::Viewport,
		EditorShortcutScope::SceneHierarchy,
		EditorShortcutScope::ContentBrowser,
		EditorShortcutScope::AssetEditor,
		EditorShortcutScope::AnimationEditor,
		EditorShortcutScope::Console,
		EditorShortcutScope::Assistant,
		EditorShortcutScope::Statistics,
		EditorShortcutScope::ProjectHub
	};

	for (EditorShortcutScope scope : Scopes)
	{
		if (m_SettingsScopeFilter >= 0 && static_cast<int>(scope) != m_SettingsScopeFilter)
			continue;

		bool hasVisibleShortcut = false;
		for (const EditorShortcut& shortcut : m_Shortcuts)
		{
			if (shortcut.m_Scope == scope && !shortcut.m_Options.m_HiddenFromSettings && TextMatchesFilter(shortcut, m_SearchBuffer))
			{
				hasVisibleShortcut = true;
				break;
			}
		}

		if (!hasVisibleShortcut)
			continue;

		ImGui::Spacing();
		if (!ImGui::CollapsingHeader(GetScopeName(scope), ImGuiTreeNodeFlags_DefaultOpen))
			continue;

		const std::string tableId = std::string("##ShortcutTable") + GetScopeName(scope);
		if (ImGui::BeginTable(tableId.c_str(), 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp))
		{
			ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 180.0f);
			ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthFixed, 110.0f);
			ImGui::TableSetupColumn("Binding", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 116.0f);
			ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 180.0f);
			ImGui::TableHeadersRow();

			for (EditorShortcut& shortcut : m_Shortcuts)
			{
				if (shortcut.m_Scope != scope || shortcut.m_Options.m_HiddenFromSettings || !TextMatchesFilter(shortcut, m_SearchBuffer))
					continue;

				const bool conflict = HasConflict(shortcut);
				ImGui::PushID(shortcut.m_Id.c_str());
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(shortcut.m_DisplayName.c_str());
				ImGui::TableNextColumn();
				ImGui::TextDisabled("%s", shortcut.m_Category.c_str());
				ImGui::TableNextColumn();
				const bool capturing = m_CapturingShortcutId == shortcut.m_Id;
				std::string bindingLabel = capturing ? "Press shortcut..." : ShortcutLabel(shortcut.m_Binding);
				if (capturing)
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.28f, 0.43f, 0.55f, 1.0f));
				if (ImGui::Button(bindingLabel.c_str(), ImVec2(-1.0f, 0.0f)))
				{
					m_CapturingShortcutId = shortcut.m_Id;
				}
				DrawShortcutTooltip(shortcut.m_Id, capturing ? "Press the desired key combination. Modifier-only presses are ignored." : "Click to record a new shortcut.");
				if (capturing)
					ImGui::PopStyleColor();
				if (capturing)
				{
					ImGui::SameLine();
					if (ImGui::SmallButton("Cancel"))
						CancelBindingCapture();
				}
				ImGui::TableNextColumn();
				if (ImGui::SmallButton("Reset"))
				{
					shortcut.m_Binding = shortcut.m_DefaultBinding;
					if (capturing)
						CancelBindingCapture();
					MarkDirty();
				}
				ImGui::SameLine();
				if (ImGui::SmallButton("Clear"))
				{
					shortcut.m_Binding = {};
					if (capturing)
						CancelBindingCapture();
					MarkDirty();
				}
				ImGui::TableNextColumn();
				if (conflict)
					ImGui::TextColored(ImVec4(0.95f, 0.50f, 0.34f, 1.0f), "%s", GetConflictDescription(shortcut).c_str());
				else
					ImGui::TextDisabled("%s", ShortcutLabel(shortcut.m_Binding).c_str());
				ImGui::PopID();
			}

			ImGui::EndTable();
		}
	}
}

bool EditorShortcutManager::ConsumeDirty()
{
	const bool dirty = m_Dirty;
	m_Dirty = false;
	return dirty;
}

void EditorShortcutManager::ResetToDefaults()
{
	for (EditorShortcut& shortcut : m_Shortcuts)
		shortcut.m_Binding = shortcut.m_DefaultBinding;
	MarkDirty();
}

void EditorShortcutManager::ResetScopeToDefaults(EditorShortcutScope scope)
{
	for (EditorShortcut& shortcut : m_Shortcuts)
		if (shortcut.m_Scope == scope)
			shortcut.m_Binding = shortcut.m_DefaultBinding;
	MarkDirty();
}

void EditorShortcutManager::LoadBindings(const YAML::Node& shortcuts)
{
	if (!shortcuts)
		return;

	for (auto it = shortcuts.begin(); it != shortcuts.end(); ++it)
	{
		const std::string id = it->first.as<std::string>("");
		const YAML::Node bindingNode = it->second;
		if (id.empty() || !bindingNode)
			continue;

		UI::ShortcutBinding binding;
		binding.m_Key = static_cast<KeyCode>(bindingNode["key"].as<int>(0));
		binding.m_Ctrl = bindingNode["ctrl"].as<bool>(false);
		binding.m_Shift = bindingNode["shift"].as<bool>(false);
		binding.m_Alt = bindingNode["alt"].as<bool>(false);
		m_PendingBindings[id] = binding;
	}

	for (EditorShortcut& shortcut : m_Shortcuts)
	{
		auto pendingIt = m_PendingBindings.find(shortcut.m_Id);
		if (pendingIt == m_PendingBindings.end())
			continue;

		shortcut.m_Binding = pendingIt->second;
		m_PendingBindings.erase(pendingIt);
	}
}

void EditorShortcutManager::SaveBindings(YAML::Emitter& out) const
{
	out << YAML::BeginMap;
	for (const EditorShortcut& shortcut : m_Shortcuts)
	{
		if (shortcut.m_Options.m_HiddenFromSettings)
			continue;
		out << YAML::Key << shortcut.m_Id << YAML::Value;
		WriteBinding(out, shortcut.m_Binding);
	}
	out << YAML::EndMap;
}

void EditorShortcutManager::SyncLegacyGlobalBindings(const UI::UISettings& settings)
{
	for (size_t i = 0; i < UI::UISettings::ActionCount; ++i)
	{
		const UI::EditorShortcutAction action = static_cast<UI::EditorShortcutAction>(i);
		const std::string id = std::string("global.") + UI::UISettings::GetActionStorageKey(action);
		if (EditorShortcut* shortcut = FindShortcut(id))
			shortcut->m_Binding = settings.GetShortcutBinding(action);
	}
}

bool EditorShortcutManager::ExecuteShortcut(std::string_view id, bool hasActiveWidget, bool ignoreTextInput, bool ignoreContext) const
{
	const EditorShortcut* shortcut = FindShortcut(id);
	return shortcut && Execute(*shortcut, hasActiveWidget, ignoreTextInput, ignoreContext);
}

bool EditorShortcutManager::IsShortcutAvailable(std::string_view id, bool requireActiveContext) const
{
	const EditorShortcut* shortcut = FindShortcut(id);
	if (!shortcut)
		return false;
	if (requireActiveContext && shortcut->m_Scope != EditorShortcutScope::Global && !IsShortcutActive(*shortcut))
		return false;
	if (shortcut->m_IsAvailable && !shortcut->m_IsAvailable())
		return false;
	return !HasConflict(*shortcut);
}

UI::ShortcutBinding EditorShortcutManager::GetBinding(std::string_view id, const UI::ShortcutBinding& fallback) const
{
	const EditorShortcut* shortcut = FindShortcut(id);
	return shortcut ? shortcut->m_Binding : fallback;
}

std::string EditorShortcutManager::GetShortcutLabel(std::string_view id) const
{
	const EditorShortcut* shortcut = FindShortcut(id);
	return shortcut ? ShortcutLabel(shortcut->m_Binding) : std::string{};
}

bool EditorShortcutManager::HasConflict(std::string_view id) const
{
	const EditorShortcut* shortcut = FindShortcut(id);
	return shortcut && HasConflict(*shortcut);
}

std::string EditorShortcutManager::GetConflictDescription(std::string_view id) const
{
	const EditorShortcut* shortcut = FindShortcut(id);
	return shortcut ? GetConflictDescription(*shortcut) : std::string{};
}

void EditorShortcutManager::CancelBindingCapture()
{
	m_CapturingShortcutId.clear();
}

void EditorShortcutManager::DrawShortcutTooltip(std::string_view id, const char* description) const
{
	if (!ImGui::IsItemHovered())
		return;

	const EditorShortcut* shortcut = FindShortcut(id);
	if (!shortcut)
		return;

	ImGui::BeginTooltip();
	if (description && description[0] != '\0')
	{
		ImGui::TextUnformatted(description);
		ImGui::Separator();
	}
	ImGui::TextDisabled("%s / %s", GetScopeName(shortcut->m_Scope), shortcut->m_Category.c_str());
	ImGui::TextUnformatted(shortcut->m_DisplayName.c_str());
	const std::string label = ShortcutLabel(shortcut->m_Binding);
	if (HasConflict(*shortcut))
		ImGui::TextColored(ImVec4(0.95f, 0.50f, 0.34f, 1.0f), "%s", GetConflictDescription(*shortcut).c_str());
	else
		ImGui::TextDisabled("Shortcut: %s", label.c_str());
	ImGui::EndTooltip();
}

const char* EditorShortcutManager::GetScopeName(EditorShortcutScope scope)
{
	switch (scope)
	{
	case EditorShortcutScope::Global: return "Global";
	case EditorShortcutScope::Viewport: return "Viewport";
	case EditorShortcutScope::SceneHierarchy: return "Scene Hierarchy";
	case EditorShortcutScope::ContentBrowser: return "Content Browser";
	case EditorShortcutScope::AssetEditor: return "Asset Editor";
	case EditorShortcutScope::AnimationEditor: return "Animation Editor";
	case EditorShortcutScope::Console: return "Console";
	case EditorShortcutScope::Assistant: return "Whip Assistant";
	case EditorShortcutScope::Statistics: return "Statistics";
	case EditorShortcutScope::ProjectHub: return "Project Hub";
	default: return "Unknown";
	}
}

std::string EditorShortcutManager::ShortcutLabel(const UI::ShortcutBinding& binding)
{
	if (binding.m_Key == 0)
		return "None";

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

EditorShortcut* EditorShortcutManager::FindShortcut(std::string_view id)
{
	const auto it = std::ranges::find_if(m_Shortcuts, [id](const EditorShortcut& shortcut)
	{
		return shortcut.m_Id == id;
	});
	return it == m_Shortcuts.end() ? nullptr : &*it;
}

const EditorShortcut* EditorShortcutManager::FindShortcut(std::string_view id) const
{
	const auto it = std::ranges::find_if(m_Shortcuts, [id](const EditorShortcut& shortcut)
	{
		return shortcut.m_Id == id;
	});
	return it == m_Shortcuts.end() ? nullptr : &*it;
}

bool EditorShortcutManager::IsShortcutActive(const EditorShortcut& shortcut) const
{
	return !shortcut.m_IsContextActive || shortcut.m_IsContextActive();
}

bool EditorShortcutManager::HandleShortcut(KeyCode key, bool ctrl, bool shift, bool alt, bool hasActiveWidget) const
{
	bool activePanelMatched = false;
	for (const EditorShortcut& shortcut : m_Shortcuts)
	{
		if (shortcut.m_Scope == EditorShortcutScope::Global || !IsShortcutActive(shortcut) || !Matches(shortcut, key, ctrl, shift, alt))
			continue;

		activePanelMatched = true;
		if (Execute(shortcut, hasActiveWidget))
			return true;
	}

	if (activePanelMatched)
		return true;

	for (const EditorShortcut& shortcut : m_Shortcuts)
	{
		if (shortcut.m_Scope != EditorShortcutScope::Global || !Matches(shortcut, key, ctrl, shift, alt))
			continue;
		if (Execute(shortcut, hasActiveWidget))
			return true;
	}

	return false;
}

bool EditorShortcutManager::Matches(const EditorShortcut& shortcut, KeyCode key, bool ctrl, bool shift, bool alt) const
{
	if (shortcut.m_Binding.m_Key == 0 || HasConflict(shortcut))
		return false;

	return shortcut.m_Binding.m_Key == key &&
		shortcut.m_Binding.m_Ctrl == ctrl &&
		shortcut.m_Binding.m_Shift == shift &&
		shortcut.m_Binding.m_Alt == alt;
}

bool EditorShortcutManager::Execute(const EditorShortcut& shortcut, bool hasActiveWidget, bool ignoreTextInput, bool ignoreContext) const
{
	const bool modifiedShortcut = shortcut.m_Binding.m_Ctrl || shortcut.m_Binding.m_Shift || shortcut.m_Binding.m_Alt;
	if (!ignoreTextInput && ImGui::GetIO().WantTextInput && !shortcut.m_Options.m_AllowWhenTextInput &&
		(!modifiedShortcut || IsTextEditingBinding(shortcut.m_Binding)))
		return false;

	if (hasActiveWidget && !modifiedShortcut && !shortcut.m_Options.m_AllowWhenActiveWidget)
		return false;

	if (!ignoreContext && shortcut.m_Scope != EditorShortcutScope::Global && !IsShortcutActive(shortcut))
		return false;

	if (shortcut.m_IsAvailable && !shortcut.m_IsAvailable())
		return false;

	return shortcut.m_Callback ? shortcut.m_Callback() : false;
}

bool EditorShortcutManager::HasConflict(const EditorShortcut& shortcut) const
{
	for (const EditorShortcut& other : m_Shortcuts)
	{
		if (&other == &shortcut || other.m_Scope != shortcut.m_Scope)
			continue;
		if (SameBinding(shortcut.m_Binding, other.m_Binding))
			return true;
	}
	return false;
}

std::string EditorShortcutManager::GetConflictDescription(const EditorShortcut& shortcut) const
{
	for (const EditorShortcut& other : m_Shortcuts)
	{
		if (&other == &shortcut || other.m_Scope != shortcut.m_Scope)
			continue;
		if (SameBinding(shortcut.m_Binding, other.m_Binding))
			return std::string("Conflicts with ") + other.m_DisplayName;
	}
	return {};
}

void EditorShortcutManager::MarkDirty()
{
	m_Dirty = true;
}

_WHIP_END
