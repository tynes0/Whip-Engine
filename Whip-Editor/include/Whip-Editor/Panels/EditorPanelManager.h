#pragma once

#include <Whip-Editor/Panels/EditorPanel.h>

#include <vector>

_WHIP_START

class EditorPanelManager
{
public:
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
