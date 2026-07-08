#pragma once

#include <Whip-Editor/Managers/EditorExportManager.h>
#include <Whip-Editor/Panels/EditorPanel.h>

_WHIP_START

class ExportPanel : public EditorPanel
{
public:
	ExportPanel();

	void SetExportManager(EditorExportManager* manager);
	void OnImGuiRender() override;
	void RegisterShortcuts(EditorShortcutManager& shortcutManager) override;

	void Open();

private:
	void RefreshDefaultsIfNeeded();
	void DrawPathRow();
	void DrawOptions();
	void DrawActions();
	void DrawProgress();
	void DrawLastBuild();

	EditorExportManager* m_ExportManager = nullptr;
	EditorExportSettings m_Settings;
	bool m_DefaultsInitialized = false;
	bool m_KeepProductNameInSync = true;
};

_WHIP_END
