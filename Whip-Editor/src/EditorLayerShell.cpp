#include <Whip-Editor/EditorLayer.h>

#include <Whip-Editor/UI/UIHelpers.h>

#include <Whip/Asset/TextureImporter.h>
#include <Whip/Core/Application.h>
#include <Whip/Project/Project.h>

#include <algorithm>
#include <cctype>
#include <string>

#include <imgui.h>

_WHIP_START

namespace
{
	enum class ShellWindowControl
	{
		Minimize,
		Maximize,
		Restore,
		Close
	};

	bool DrawShellWindowControlButton(const char* id, ShellWindowControl control, ImVec2 size)
	{
		ImGui::InvisibleButton(id, size);
		const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
		const bool hovered = ImGui::IsItemHovered();
		const bool active = ImGui::IsItemActive();

		const ImVec2 min = ImGui::GetItemRectMin();
		const ImVec2 max = ImGui::GetItemRectMax();
		const ImVec2 center((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
		ImDrawList* drawList = ImGui::GetWindowDrawList();

		ImU32 background = IM_COL32(255, 255, 255, hovered ? 28 : 0);
		if (control == ShellWindowControl::Close && hovered)
			background = IM_COL32(196, 58, 46, active ? 230 : 205);
		else if (active)
			background = IM_COL32(255, 255, 255, 42);

		drawList->AddRectFilled(min, max, background, 0.0f);
		const ImU32 iconColor = control == ShellWindowControl::Close && hovered ? IM_COL32(255, 244, 234, 255) : IM_COL32(226, 218, 202, 235);

		switch (control)
		{
		case ShellWindowControl::Minimize:
			drawList->AddLine(ImVec2(center.x - 5.0f, center.y + 5.0f), ImVec2(center.x + 5.0f, center.y + 5.0f), iconColor, 1.35f);
			break;
		case ShellWindowControl::Maximize:
			drawList->AddRect(ImVec2(center.x - 5.0f, center.y - 5.0f), ImVec2(center.x + 5.0f, center.y + 5.0f), iconColor, 0.0f, 0, 1.25f);
			break;
		case ShellWindowControl::Restore:
			drawList->AddRect(ImVec2(center.x - 3.0f, center.y - 6.0f), ImVec2(center.x + 6.0f, center.y + 3.0f), iconColor, 0.0f, 0, 1.1f);
			drawList->AddRect(ImVec2(center.x - 7.0f, center.y - 2.0f), ImVec2(center.x + 2.0f, center.y + 7.0f), iconColor, 0.0f, 0, 1.1f);
			break;
		case ShellWindowControl::Close:
			drawList->AddLine(ImVec2(center.x - 5.0f, center.y - 5.0f), ImVec2(center.x + 5.0f, center.y + 5.0f), iconColor, 1.35f);
			drawList->AddLine(ImVec2(center.x + 5.0f, center.y - 5.0f), ImVec2(center.x - 5.0f, center.y + 5.0f), iconColor, 1.35f);
			break;
		}

		return clicked;
	}

	Ref<Texture2D> GetWhipBrandTexture()
	{
		static Ref<Texture2D> texture = TextureImporter::LoadTexture2D("resources/icons/whip_editor_logo.png");
		return texture;
	}

	void DrawWhipBrandMark(ImDrawList* drawList, const ImVec2& min)
	{
		const ImVec2 max(min.x + 20.0f, min.y + 20.0f);
		if (Ref<Texture2D> texture = GetWhipBrandTexture(); texture && texture->IsLoaded())
		{
			drawList->AddImage(UI::ToImGuiTextureId(texture->GetRendererId()), min, max, ImVec2(0, 1), ImVec2(1, 0));
			return;
		}

		const ImVec2 center((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
		const ImVec2 mark[] =
		{
			ImVec2(center.x, min.y + 2.0f),
			ImVec2(max.x - 3.0f, center.y),
			ImVec2(center.x, max.y - 2.0f),
			ImVec2(min.x + 3.0f, center.y)
		};
		drawList->AddConvexPolyFilled(mark, 4, IM_COL32(245, 248, 252, 245));
		drawList->AddPolyline(mark, 4, IM_COL32(110, 128, 146, 190), ImDrawFlags_Closed, 1.2f);
	}

	std::string LowerCopy(std::string value)
	{
		std::ranges::transform(value, value.begin(),
		                       [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
		return value;
	}

	constexpr UI::EditorShortcutAction CommandPaletteActions[] =
	{
		UI::EditorShortcutAction::OpenProject,
		UI::EditorShortcutAction::NewScene,
		UI::EditorShortcutAction::SaveScene,
		UI::EditorShortcutAction::SaveSceneAs,
		UI::EditorShortcutAction::SaveProject,
		UI::EditorShortcutAction::CloseScene,
		UI::EditorShortcutAction::Undo,
		UI::EditorShortcutAction::Redo,
		UI::EditorShortcutAction::SelectAll,
		UI::EditorShortcutAction::Copy,
		UI::EditorShortcutAction::Paste,
		UI::EditorShortcutAction::Cut,
		UI::EditorShortcutAction::DuplicateEntity,
		UI::EditorShortcutAction::DeleteEntity,
		UI::EditorShortcutAction::Play,
		UI::EditorShortcutAction::Simulate,
		UI::EditorShortcutAction::Stop,
		UI::EditorShortcutAction::Pause,
		UI::EditorShortcutAction::GizmoNone,
		UI::EditorShortcutAction::GizmoTranslate,
		UI::EditorShortcutAction::GizmoRotate,
		UI::EditorShortcutAction::GizmoScale,
		UI::EditorShortcutAction::ReloadScripts,
		UI::EditorShortcutAction::OpenSettings,
		UI::EditorShortcutAction::OpenCommandPalette
	};

	bool CommandMatchesFilter(UI::EditorShortcutAction action, const char* filter)
	{
		if (!filter || filter[0] == '\0')
			return true;

		std::string needle = LowerCopy(filter);
		std::string haystack = LowerCopy(std::string(UI::UISettings::GetActionDisplayName(action)) + " " + UI::UISettings::GetActionCategory(action));
		return haystack.find(needle) != std::string::npos;
	}
}

void EditorLayer::DrawEditorShellTitlebar(bool projectLoaded)
{
	constexpr float TitlebarHeight = 30.0f;
	constexpr float ControlWidth = 46.0f;

	ImGui::BeginChild("##EditorShellTitlebar", ImVec2(0.0f, TitlebarHeight), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	const ImVec2 min = ImGui::GetWindowPos();
	const ImVec2 size = ImGui::GetWindowSize();
	const ImVec2 max(min.x + size.x, min.y + size.y);
	ImDrawList* drawList = ImGui::GetWindowDrawList();

	const ImU32 titleTop = IM_COL32(21, 29, 37, 255);
	const ImU32 titleBottom = IM_COL32(8, 12, 16, 255);
	drawList->AddRectFilledMultiColor(min, max, titleTop, titleTop, titleBottom, titleBottom);
	drawList->AddLine(ImVec2(min.x, max.y - 1.0f), ImVec2(max.x, max.y - 1.0f), IM_COL32(46, 58, 70, 210), 1.0f);
	drawList->AddRectFilled(ImVec2(min.x, min.y), ImVec2(max.x, min.y + 2.0f), IM_COL32(180, 196, 214, 210), 0.0f);

	const float controlStartX = max.x - ControlWidth * 3.0f;

	const ImVec2 logoMin(min.x + 12.0f, min.y + 6.0f);
	DrawWhipBrandMark(drawList, logoMin);

	std::string title = "Whip Editor";
	if (projectLoaded && Project::GetActive())
		title += "  /  " + Project::GetActive()->GetConfig().m_Name;
	else
		title += "  /  Hub";

	const float titleX = logoMin.x + 40.0f;
	drawList->AddText(ImVec2(titleX, min.y + 5.0f), IM_COL32(240, 244, 248, 245), title.c_str());
	drawList->AddText(ImVec2(titleX + ImGui::CalcTextSize(title.c_str()).x + 10.0f, min.y + 5.0f), IM_COL32(132, 150, 166, 210), projectLoaded ? "Editor" : "Project Launcher");

	Window& window = Application::Get().GetWindow();
	ImGui::SetCursorScreenPos(ImVec2(controlStartX, min.y));
	if (DrawShellWindowControlButton("##ShellMinimize", ShellWindowControl::Minimize, ImVec2(ControlWidth, TitlebarHeight)))
		window.Minimize();
	ImGui::SameLine(0.0f, 0.0f);
	if (DrawShellWindowControlButton("##ShellMaximize", window.IsMaximized() ? ShellWindowControl::Restore : ShellWindowControl::Maximize, ImVec2(ControlWidth, TitlebarHeight)))
	{
		if (window.IsMaximized())
			window.Restore();
		else
			window.Maximize();
	}
	ImGui::SameLine(0.0f, 0.0f);
	if (DrawShellWindowControlButton("##ShellClose", ShellWindowControl::Close, ImVec2(ControlWidth, TitlebarHeight)))
		Application::Get().Close();

	ImGui::EndChild();
}

void EditorLayer::DrawEditorMenuBar(bool projectLoaded)
{
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.040f, 0.055f, 0.070f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4(0.040f, 0.055f, 0.070f, 1.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::BeginChild("##EditorShellMenuBar", ImVec2(0.0f, 28.0f), false, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	ImGui::PopStyleVar();

	if (ImGui::BeginMenuBar())
	{
		auto drawMenuAction = [this](UI::EditorShortcutAction action, const char* label = nullptr)
			{
				std::string shortcut = m_UISettings.GetShortcutLabel(action);
				const bool available = IsEditorActionAvailable(action);
				ImGui::BeginDisabled(!available);
				bool clicked = ImGui::MenuItem(label ? label : UI::UISettings::GetActionDisplayName(action), shortcut.c_str());
				ImGui::EndDisabled();
				if (clicked)
					ExecuteEditorAction(action);
			};

		if (ImGui::BeginMenu("File"))
		{
			drawMenuAction(UI::EditorShortcutAction::OpenProject);
			drawMenuAction(UI::EditorShortcutAction::SaveProject);
			ImGui::Separator();
			drawMenuAction(UI::EditorShortcutAction::NewScene);
			drawMenuAction(UI::EditorShortcutAction::SaveScene);
			drawMenuAction(UI::EditorShortcutAction::SaveSceneAs, "Save Scene As...");
			drawMenuAction(UI::EditorShortcutAction::CloseScene);
			ImGui::Separator();
			if (ImGui::MenuItem("Restart"))
				Application::Get().SubmitToNextTick([]() { Application::Get().Restart(); });
			if (ImGui::MenuItem("Exit"))
				Application::Get().Close();
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Edit"))
		{
			drawMenuAction(UI::EditorShortcutAction::OpenCommandPalette);
			drawMenuAction(UI::EditorShortcutAction::OpenSettings, "Settings");
			ImGui::Separator();
			drawMenuAction(UI::EditorShortcutAction::Undo);
			drawMenuAction(UI::EditorShortcutAction::Redo);
			ImGui::Separator();
			drawMenuAction(UI::EditorShortcutAction::SelectAll);
			drawMenuAction(UI::EditorShortcutAction::Copy);
			drawMenuAction(UI::EditorShortcutAction::Paste);
			drawMenuAction(UI::EditorShortcutAction::Cut);
			drawMenuAction(UI::EditorShortcutAction::DuplicateEntity);
			drawMenuAction(UI::EditorShortcutAction::DeleteEntity);
			ImGui::Separator();
			ImGui::BeginDisabled(!projectLoaded);
			if (ImGui::MenuItem("Show Animation Editor"))
				m_AnimationEditorPanel.Open();
			ImGui::EndDisabled();
			if (ImGui::MenuItem("Show Test Popup"))
				m_PopupHandler.SetShowState(true);
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Script"))
		{
			drawMenuAction(UI::EditorShortcutAction::ReloadScripts, "Reload Assembly");
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Project"))
		{
			drawMenuAction(UI::EditorShortcutAction::OpenSettings, "Settings");
			ImGui::Separator();
			drawMenuAction(UI::EditorShortcutAction::OpenProject);
			drawMenuAction(UI::EditorShortcutAction::SaveProject);
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Window"))
		{
			RebuildEditorPanelRegistry();
			m_PanelManager.DrawAddPanelMenu(projectLoaded);
			ImGui::Separator();
			ImGui::BeginDisabled(!projectLoaded);
			ImGui::BeginDisabled(!m_AssetEditorPanel.HasOpenEditors());
			if (ImGui::MenuItem("Close Asset Editors"))
				m_AssetEditorPanel.CloseAll();
			ImGui::EndDisabled();
			ImGui::EndDisabled();
			ImGui::EndMenu();
		}

		ImGui::EndMenuBar();
	}

	ImGui::EndChild();
	ImGui::PopStyleColor(2);
}

void EditorLayer::OpenCommandPalette()
{
	m_CommandPaletteOpen = true;
	m_CommandPaletteFocusSearch = true;
	m_CommandPaletteFilter[0] = '\0';
}

void EditorLayer::DrawCommandPalette()
{
	if (!m_CommandPaletteOpen)
		return;

	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f, viewport->WorkPos.y + viewport->WorkSize.y * 0.22f), ImGuiCond_Appearing, ImVec2(0.5f, 0.0f));
	ImGui::SetNextWindowSize(ImVec2(680.0f, 460.0f), ImGuiCond_Appearing);

	bool open = true;
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;
	if (ImGui::Begin("Command Palette", &open, flags))
	{
		if (m_CommandPaletteFocusSearch)
		{
			ImGui::SetKeyboardFocusHere();
			m_CommandPaletteFocusSearch = false;
		}

		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputTextWithHint("##CommandPaletteSearch", "Search commands...", m_CommandPaletteFilter, sizeof(m_CommandPaletteFilter));
		ImGui::Spacing();
		ImGui::Separator();

		UI::EditorShortcutAction firstAvailableAction = UI::EditorShortcutAction::Count;
		bool hasVisibleCommand = false;

		if (ImGui::BeginChild("##CommandPaletteResults", ImVec2(0.0f, 0.0f), false))
		{
			if (ImGui::BeginTable("##CommandPaletteTable", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
			{
				ImGui::TableSetupColumn("Command", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("Group", ImGuiTableColumnFlags_WidthFixed, 110.0f);
				ImGui::TableSetupColumn("Shortcut", ImGuiTableColumnFlags_WidthFixed, 150.0f);

				for (UI::EditorShortcutAction action : CommandPaletteActions)
				{
					if (!CommandMatchesFilter(action, m_CommandPaletteFilter))
						continue;

					hasVisibleCommand = true;
					const bool available = IsEditorActionAvailable(action);
					if (available && firstAvailableAction == UI::EditorShortcutAction::Count)
						firstAvailableAction = action;

					ImGui::PushID(static_cast<int>(action));
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::BeginDisabled(!available);
					if (ImGui::Selectable(UI::UISettings::GetActionDisplayName(action), false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0.0f, 30.0f)))
					{
						if (ExecuteEditorAction(action) && action != UI::EditorShortcutAction::OpenCommandPalette)
							m_CommandPaletteOpen = false;
					}
					ImGui::TableNextColumn();
					ImGui::TextDisabled("%s", UI::UISettings::GetActionCategory(action));
					ImGui::TableNextColumn();
					const std::string shortcut = m_UISettings.GetShortcutLabel(action);
					if (m_UISettings.HasShortcutConflict(action))
						ImGui::TextColored(ImVec4(0.95f, 0.50f, 0.34f, 1.0f), "Conflict");
					else
						ImGui::TextDisabled("%s", shortcut.c_str());
					ImGui::EndDisabled();
					ImGui::PopID();
				}

				ImGui::EndTable();
			}

			if (!hasVisibleCommand)
				ImGui::TextDisabled("No commands found.");
		}
		ImGui::EndChild();

		if (ImGui::IsKeyPressed(ImGuiKey_Escape))
			m_CommandPaletteOpen = false;
		if (firstAvailableAction != UI::EditorShortcutAction::Count && ImGui::IsKeyPressed(ImGuiKey_Enter))
		{
			if (ExecuteEditorAction(firstAvailableAction) && firstAvailableAction != UI::EditorShortcutAction::OpenCommandPalette)
				m_CommandPaletteOpen = false;
		}
	}
	ImGui::End();

	if (!open)
		m_CommandPaletteOpen = false;
}

_WHIP_END
