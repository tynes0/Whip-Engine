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
	void DrawMetadata();
	void DrawConfigurationRow();
	void DrawPathRow();
	void DrawOptions();
	void DrawActions();
	void DrawProgress();
	void DrawLastBuild();
	void DrawBuildLogPreview();

	EditorExportManager* m_ExportManager = nullptr;
	EditorExportSettings m_Settings;
	std::string m_LastBuildLogPreview;
	bool m_DefaultsInitialized = false;
	bool m_KeepProductNameInSync = true;
};

_WHIP_END
