#include <WhipPch.h>

#include <Whip-Editor/Panels/ExportPanel.h>

#include <Whip-Editor/Managers/EditorShortcutManager.h>

#include <Whip/Project/Project.h>
#include <Whip/Utils/PlatformUtils.h>

#include <algorithm>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

_WHIP_START

namespace
{
	std::string PathLabel(const std::filesystem::path& path)
	{
		return path.empty() ? std::string("Choose folder") : path.string();
	}
}

ExportPanel::ExportPanel()
	: EditorPanel("Build & Export", false, true)
{
}

void ExportPanel::SetExportManager(EditorExportManager* manager)
{
	m_ExportManager = manager;
}

void ExportPanel::Open()
{
	SetOpen(true);
	RefreshDefaultsIfNeeded();
}

void ExportPanel::OnImGuiRender()
{
	WHP_PROFILE_FUNCTION();
	if (!m_Open)
		return;

	RefreshDefaultsIfNeeded();

	bool open = m_Open;
	ImGui::SetNextWindowSize(ImVec2(760.0f, 480.0f), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Build & Export", &open))
	{
		SetOpen(open);
		ImGui::End();
		return;
	}
	SetOpen(open);

	if (!Project::GetActive() || !Project::Loaded())
	{
		ImGui::TextDisabled("Open a project to export a playable build.");
		ImGui::End();
		return;
	}

	ImGui::TextUnformatted("Windows Player");
	ImGui::SameLine();
	ImGui::TextDisabled("Desktop x86_64");
	ImGui::Separator();

	ImGui::BeginDisabled(m_ExportManager && m_ExportManager->IsExportRunning());
	ImGui::SetNextItemWidth(-1.0f);
	if (ImGui::InputText("Product", &m_Settings.m_ProductName))
		m_KeepProductNameInSync = false;
	DrawConfigurationRow();
	DrawPathRow();
	DrawOptions();
	ImGui::EndDisabled();

	ImGui::Separator();
	DrawActions();
	DrawProgress();
	DrawLastBuild();

	ImGui::End();
}

void ExportPanel::RegisterShortcuts(EditorShortcutManager& shortcutManager)
{
	EditorShortcutOptions options;
	options.m_AllowWhenActiveWidget = true;
	shortcutManager.Add(
		EditorShortcutScope::Global,
		"window.open_export_panel",
		"Open Build & Export",
		"Window",
		{ Key::B, true, true, false },
		[this]()
		{
			Open();
			return true;
		},
		[]() { return Project::GetActive() && Project::Loaded(); },
		{},
		options);

	shortcutManager.Add(
		EditorShortcutScope::Global,
		"project.export_windows",
		"Export Windows Player",
		"Project",
		{ Key::B, true, false, false },
		[this]()
		{
			if (!m_ExportManager)
				return false;
			RefreshDefaultsIfNeeded();
			return m_ExportManager->BeginExport(m_Settings);
		},
		[this]() { return m_ExportManager && !m_ExportManager->IsExportRunning() && Project::GetActive() && Project::Loaded(); },
		{},
		options);
}

void ExportPanel::RefreshDefaultsIfNeeded()
{
	if (!m_ExportManager)
		return;

	EditorExportSettings defaults = m_ExportManager->MakeDefaultSettings();
	if (!m_DefaultsInitialized)
	{
		m_Settings = defaults;
		m_DefaultsInitialized = true;
		return;
	}

	if (m_Settings.m_OutputRoot.empty())
		m_Settings.m_OutputRoot = defaults.m_OutputRoot;
	if (m_KeepProductNameInSync)
		m_Settings.m_ProductName = defaults.m_ProductName;
}

void ExportPanel::DrawPathRow()
{
	ImGui::TextUnformatted("Output");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(-96.0f);
	std::string outputRoot = PathLabel(m_Settings.m_OutputRoot);
	ImGui::InputText("##ExportOutputRoot", &outputRoot, ImGuiInputTextFlags_ReadOnly);
	ImGui::SameLine();
	if (ImGui::Button("Browse", ImVec2(84.0f, 0.0f)))
	{
		std::string folder = FileDialogs::OpenFolder();
		if (!folder.empty())
			m_Settings.m_OutputRoot = folder;
	}
}

void ExportPanel::DrawConfigurationRow()
{
	const char* currentName = EditorExportManager::GetConfigurationName(m_Settings.m_Configuration);
	ImGui::SetNextItemWidth(180.0f);
	if (!ImGui::BeginCombo("Configuration", currentName))
		return;

	for (EditorExportConfiguration configuration : { EditorExportConfiguration::Debug, EditorExportConfiguration::Release })
	{
		const bool selected = m_Settings.m_Configuration == configuration;
		if (ImGui::Selectable(EditorExportManager::GetConfigurationName(configuration), selected))
			m_Settings.m_Configuration = configuration;
		if (selected)
			ImGui::SetItemDefaultFocus();
	}

	ImGui::EndCombo();
	ImGui::SameLine();
	ImGui::TextDisabled("Preset: %s", EditorExportManager::GetBuildPresetName(m_Settings.m_Configuration));
}

void ExportPanel::DrawOptions()
{
	if (ImGui::BeginTable("##ExportOptions", 2, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_PadOuterX))
	{
		ImGui::TableNextColumn();
		ImGui::Checkbox("Build native player", &m_Settings.m_BuildNativePlayer);
		ImGui::Checkbox("Clean output folder", &m_Settings.m_CleanOutputDirectory);
		ImGui::Checkbox("Build scripts before export", &m_Settings.m_BuildScripts);
		ImGui::TableNextColumn();
		ImGui::Checkbox("Run after export", &m_Settings.m_RunAfterExport);
		ImGui::Checkbox("Open folder after export", &m_Settings.m_OpenFolderAfterExport);
		ImGui::EndTable();
	}
}

void ExportPanel::DrawActions()
{
	if (!m_ExportManager)
		return;

	const bool running = m_ExportManager->IsExportRunning();
	ImGui::BeginDisabled(running);
	const std::string exportLabel = std::string("Export ") + EditorExportManager::GetConfigurationName(m_Settings.m_Configuration) + " Build";
	if (ImGui::Button(exportLabel.c_str(), ImVec2(180.0f, 30.0f)))
		m_ExportManager->BeginExport(m_Settings);
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::BeginDisabled(!running);
	if (ImGui::Button("Cancel", ImVec2(96.0f, 30.0f)))
		m_ExportManager->CancelExport(false);
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::TextDisabled("%s", m_ExportManager->GetStatus().c_str());
}

void ExportPanel::DrawProgress()
{
	if (!m_ExportManager || !m_ExportManager->IsExportRunning())
		return;

	const Async::JobProgressSnapshot snapshot = m_ExportManager->GetProgressSnapshot();
	const float progress = std::clamp(snapshot.m_Progress, 0.0f, 1.0f);
	ImGui::Spacing();
	ImGui::ProgressBar(progress, ImVec2(-1.0f, 12.0f), "");
	ImGui::TextDisabled("%s", snapshot.m_Message.empty() ? "Packaging..." : snapshot.m_Message.c_str());
	if (!snapshot.m_Error.empty())
		ImGui::TextColored(ImVec4(0.95f, 0.42f, 0.34f, 1.0f), "%s", snapshot.m_Error.c_str());
}

void ExportPanel::DrawLastBuild()
{
	if (!m_ExportManager || !m_ExportManager->HasLastExport())
		return;

	const EditorExportResult& result = m_ExportManager->GetLastResult();
	ImGui::Spacing();
	ImGui::SeparatorText("Last Build");
	ImGui::TextDisabled("Configuration: %s", EditorExportManager::GetConfigurationName(result.m_Configuration));
	ImGui::SameLine();
	ImGui::TextDisabled("| Native: %s", result.m_NativeBuildSucceeded ? "built" : "skipped");
	ImGui::TextWrapped("%s", result.m_OutputDirectory.string().c_str());
	if (!result.m_Warnings.empty())
	{
		for (const std::string& warning : result.m_Warnings)
			ImGui::TextColored(ImVec4(0.92f, 0.70f, 0.36f, 1.0f), "%s", warning.c_str());
	}

	if (ImGui::Button("Run", ImVec2(92.0f, 0.0f)))
		m_ExportManager->RunLastExport();
	ImGui::SameLine();
	if (ImGui::Button("Open Folder", ImVec2(118.0f, 0.0f)))
		m_ExportManager->OpenLastOutputFolder();
}

_WHIP_END
