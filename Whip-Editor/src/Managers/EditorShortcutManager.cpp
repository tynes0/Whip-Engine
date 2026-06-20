#include <WhipPch.h>

#include <Whip-Editor/Managers/EditorShortcutManager.h>

#include <Whip-Editor/EditorLayer.h>

#include <algorithm>
#include <cctype>

#include <imgui.h>

_WHIP_START

namespace
{
	constexpr KeyCode EditableKeys[] =
	{
		Key::Q, Key::W, Key::E, Key::R, Key::T, Key::Y, Key::U, Key::I, Key::A, Key::S, Key::D, Key::F, Key::G, Key::H, Key::J, Key::K, Key::L,
		Key::Z, Key::X, Key::C, Key::V, Key::B, Key::N, Key::M, Key::O, Key::P,
		Key::D0, Key::D1, Key::D2, Key::D3, Key::D4, Key::D5, Key::D6, Key::D7, Key::D8, Key::D9,
		Key::F1, Key::F2, Key::F3, Key::F4, Key::F5, Key::F6, Key::F7, Key::F8, Key::F9, Key::F10, Key::F11, Key::F12,
		Key::Escape, Key::Delete, Key::Backspace, Key::Insert, Key::Space, Key::Enter, Key::Tab,
		Key::Left, Key::Right, Key::Up, Key::Down, Key::Home, Key::End, Key::PageUp, Key::PageDown,
		Key::Comma, Key::Period, Key::Minus, Key::Equal, Key::LeftBracket, Key::RightBracket, Key::GraveAccent
	};

	bool SameBinding(const UI::ShortcutBinding& left, const UI::ShortcutBinding& right)
	{
		return left.m_Key != 0 &&
			left.m_Key == right.m_Key &&
			left.m_Ctrl == right.m_Ctrl &&
			left.m_Shift == right.m_Shift &&
			left.m_Alt == right.m_Alt;
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
		return false;

	const bool control = Input::IsKeyDown(Key::LeftControl) || Input::IsKeyDown(Key::RightControl);
	const bool shift = Input::IsKeyDown(Key::LeftShift) || Input::IsKeyDown(Key::RightShift);
	const bool alt = Input::IsKeyDown(Key::LeftAlt) || Input::IsKeyDown(Key::RightAlt);
	const KeyCode key = event.GetKeyCode();

	for (const EditorShortcut& shortcut : m_Shortcuts)
	{
		if (shortcut.m_Scope == EditorShortcutScope::Global || !IsShortcutActive(shortcut) || !Matches(shortcut, key, control, shift, alt))
			continue;
		if (Execute(shortcut, hasActiveWidget))
			return true;
	}

	for (const EditorShortcut& shortcut : m_Shortcuts)
	{
		if (shortcut.m_Scope != EditorShortcutScope::Global || !Matches(shortcut, key, control, shift, alt))
			continue;
		return Execute(shortcut, hasActiveWidget);
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
		if (ImGui::BeginTable(tableId.c_str(), 7, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp))
		{
			ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 180.0f);
			ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthFixed, 110.0f);
			ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Ctrl", ImGuiTableColumnFlags_WidthFixed, 54.0f);
			ImGui::TableSetupColumn("Shift", ImGuiTableColumnFlags_WidthFixed, 54.0f);
			ImGui::TableSetupColumn("Alt", ImGuiTableColumnFlags_WidthFixed, 54.0f);
			ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 120.0f);
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
				ImGui::SetNextItemWidth(-1.0f);
				if (ImGui::BeginCombo("##Key", Key::ToString(shortcut.m_Binding.m_Key)))
				{
					if (ImGui::Selectable("None", shortcut.m_Binding.m_Key == 0))
					{
						shortcut.m_Binding.m_Key = 0;
						MarkDirty();
					}
					for (KeyCode candidate : EditableKeys)
					{
						const bool selected = shortcut.m_Binding.m_Key == candidate;
						if (ImGui::Selectable(Key::ToString(candidate), selected))
						{
							shortcut.m_Binding.m_Key = candidate;
							MarkDirty();
						}
						if (selected)
							ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}
				ImGui::TableNextColumn();
				if (ImGui::Checkbox("##Ctrl", &shortcut.m_Binding.m_Ctrl))
					MarkDirty();
				ImGui::TableNextColumn();
				if (ImGui::Checkbox("##Shift", &shortcut.m_Binding.m_Shift))
					MarkDirty();
				ImGui::TableNextColumn();
				if (ImGui::Checkbox("##Alt", &shortcut.m_Binding.m_Alt))
					MarkDirty();
				ImGui::TableNextColumn();
				if (conflict)
					ImGui::TextColored(ImVec4(0.95f, 0.50f, 0.34f, 1.0f), "Scope conflict");
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
	if (!ignoreTextInput && ImGui::GetIO().WantTextInput && !shortcut.m_Options.m_AllowWhenTextInput)
		return false;

	if (hasActiveWidget && !shortcut.m_Options.m_AllowWhenActiveWidget)
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

void EditorShortcutManager::MarkDirty()
{
	m_Dirty = true;
}

_WHIP_END
