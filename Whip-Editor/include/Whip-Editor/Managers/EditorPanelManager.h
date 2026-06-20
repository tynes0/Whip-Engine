#pragma once

#include <Whip-Editor/Panels/EditorPanel.h>

#include <vector>

#include "EditorManagerBase.h"

_WHIP_START

class EditorPanelManager : public EditorManagerBase // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	EditorPanelManager(EditorLayer* boundedLayer = nullptr);
	~EditorPanelManager() override;

	void Clear();
	void AddPanel(EditorPanel& panel);
	void OnImGuiRender();
	bool ConsumeOpenDirty();
	void DrawAddPanelMenu(bool projectLoaded);

	const std::vector<EditorPanel*>& GetPanels() const { return m_Panels; }

private:
	std::vector<EditorPanel*> m_Panels;
};

_WHIP_END
