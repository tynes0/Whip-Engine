#include <WhipPch.h>

#include <Whip-Editor/Managers/EditorPanelManager.h>

#include <Whip-Editor/EditorLayer.h>

#include <algorithm>

#include <imgui.h>

_WHIP_START

EditorPanelManager::EditorPanelManager(EditorLayer* boundedLayer)
	: EditorManagerBase(boundedLayer)
{
}

EditorPanelManager::~EditorPanelManager() = default;

void EditorPanelManager::Clear()
{
	m_Panels.clear();
}

void EditorPanelManager::AddPanel(EditorPanel& panel)
{
	if (std::ranges::find(m_Panels, &panel) == m_Panels.end())
		m_Panels.push_back(&panel);
}

void EditorPanelManager::OnImGuiRender()
{
	WHP_PROFILE_FUNCTION();
	for (EditorPanel* panel : m_Panels)
		panel->OnImGuiRender();
}

bool EditorPanelManager::ConsumeOpenDirty()
{
	bool dirty = false;
	for (EditorPanel* panel : m_Panels)
		dirty |= panel->ConsumeOpenDirty();
	return dirty;
}

void EditorPanelManager::DrawAddPanelMenu(bool projectLoaded)
{
	WHP_PROFILE_FUNCTION();
	if (ImGui::BeginMenu("Add Panel"))
	{
		for (EditorPanel* panel : m_Panels)
		{
			const bool disabled = (panel->RequiresProject() && !projectLoaded) || panel->IsOpen() || !panel->CanOpenFromMenu();
			ImGui::BeginDisabled(disabled);
			if (ImGui::MenuItem(panel->GetName().c_str()))
				panel->SetOpen(true);
			ImGui::EndDisabled();
		}
		ImGui::EndMenu();
	}

	ImGui::Separator();
	for (EditorPanel* panel : m_Panels)
	{
		if (!panel->CanOpenFromMenu() && !panel->IsOpen())
			continue;

		const bool disabled = panel->RequiresProject() && !projectLoaded;
		bool requestedOpen = panel->IsOpen();
		ImGui::BeginDisabled(disabled);
		if (ImGui::MenuItem(panel->GetName().c_str(), nullptr, &requestedOpen))
			panel->SetOpen(requestedOpen);
		ImGui::EndDisabled();
	}
}

_WHIP_END
