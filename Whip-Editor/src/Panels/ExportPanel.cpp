#include <WhipPch.h>

#include <Whip-Editor/Panels/ExportPanel.h>

#include <Whip-Editor/Managers/EditorShortcutManager.h>

#include <Whip/Project/Project.h>
#include <Whip/Utils/PlatformUtils.h>

#include <algorithm>
#include <fstream>
#include <sstream>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

_WHIP_START

namespace
{
	std::string PathLabel(const std::filesystem::path& path)
	{
		return path.empty() ? std::string("Choose folder") : path.string();
	}

	std::string ReadPreviewText(const std::filesystem::path& path, size_t maxBytes = 12000)
	{
		if (path.empty())
			return {};

		std::ifstream stream(path, std::ios::binary);
		if (!stream)
			return {};

		std::ostringstream buffer;
		buffer << stream.rdbuf();
		std::string text = buffer.str();
		if (text.size() > maxBytes)
			text = text.substr(text.size() - maxBytes);
		return text;
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
	ImGui::SetNextWindowSize(ImVec2(860.0f, 620.0f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowDockID(0, ImGuiCond_Always);
	const ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse;
	if (!ImGui::Begin("Build & Export", &open, windowFlags))
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
	DrawMetadata();
	DrawConfigurationRow();
	DrawPathRow();
	DrawOptions();
	ImGui::EndDisabled();

	ImGui::Separator();
	DrawActions();
	DrawProgress();
	DrawLastBuild();
	DrawBuildLogPreview();

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

void ExportPanel::DrawMetadata()
{
	ImGui::SeparatorText("Product");
	if (ImGui::BeginTable("##ExportProductMetadata", 2, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_PadOuterX))
	{
		ImGui::TableNextColumn();
		ImGui::TextDisabled("Name");
		ImGui::SetNextItemWidth(-1.0f);
		if (ImGui::InputText("##ExportProductName", &m_Settings.m_ProductName))
			m_KeepProductNameInSync = false;
		ImGui::Spacing();
		ImGui::TextDisabled("Version");
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputText("##ExportProductVersion", &m_Settings.m_ProductVersion);

		ImGui::TableNextColumn();
		ImGui::TextDisabled("Company");
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputText("##ExportProductCompany", &m_Settings.m_CompanyName);
		ImGui::Spacing();
		ImGui::TextDisabled("Icon");
		ImGui::SetNextItemWidth(-132.0f);
		std::string iconPath = m_Settings.m_ProductIconPath.empty() ? std::string("None") : m_Settings.m_ProductIconPath.string();
		ImGui::InputText("##ExportProductIcon", &iconPath, ImGuiInputTextFlags_ReadOnly);
		ImGui::SameLine();
		if (ImGui::Button("Browse##Icon", ImVec2(84.0f, 0.0f)))
		{
			const std::string filepath = FileDialogs::OpenFile("Icon/Image (*.ico;*.png)\0*.ico;*.png\0All Files (*.*)\0*.*\0");
			if (!filepath.empty())
				m_Settings.m_ProductIconPath = filepath;
		}
		ImGui::SameLine();
		if (ImGui::Button("X##Icon", ImVec2(32.0f, 0.0f)))
			m_Settings.m_ProductIconPath.clear();
		ImGui::EndTable();
	}
}

void ExportPanel::DrawPathRow()
{
	ImGui::SeparatorText("Output");
	ImGui::TextDisabled("Folder");
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
	ImGui::SeparatorText("Build");
	const char* currentName = EditorExportManager::GetConfigurationName(m_Settings.m_Configuration);
	ImGui::TextDisabled("Configuration");
	ImGui::SetNextItemWidth(180.0f);
	if (!ImGui::BeginCombo("##ExportConfiguration", currentName))
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
	if (ImGui::BeginTable("##ExportOptions", 3, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_PadOuterX))
	{
		ImGui::TableNextColumn();
		ImGui::Checkbox("Build native player", &m_Settings.m_BuildNativePlayer);
		ImGui::Checkbox("Clean output folder", &m_Settings.m_CleanOutputDirectory);
		ImGui::Checkbox("Build scripts before export", &m_Settings.m_BuildScripts);
		ImGui::TableNextColumn();
		ImGui::Checkbox("Project Health gate", &m_Settings.m_RunProjectHealthCheck);
		ImGui::BeginDisabled(!m_Settings.m_RunProjectHealthCheck);
		ImGui::Checkbox("Block on validation errors", &m_Settings.m_BlockOnValidationErrors);
		ImGui::EndDisabled();
		ImGui::Checkbox("Create zip package", &m_Settings.m_PackageZip);
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
	if (ImGui::Button(exportLabel.c_str(), ImVec2(190.0f, 30.0f)))
	{
		m_LastBuildLogPreview.clear();
		m_ExportManager->BeginExport(m_Settings);
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::BeginDisabled(!running);
	if (ImGui::Button("Cancel", ImVec2(96.0f, 30.0f)))
		m_ExportManager->CancelExport(false);
	ImGui::EndDisabled();
	if (ImGui::GetContentRegionAvail().x > 240.0f)
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
	ImGui::SameLine();
	ImGui::TextDisabled("| Package: %s", result.m_PackageCreated ? "created" : "none");
	ImGui::TextDisabled("Validation: %d error(s), %d warning(s)", result.m_ValidationErrors, result.m_ValidationWarnings);
	ImGui::TextWrapped("%s", result.m_OutputDirectory.string().c_str());
	if (!result.m_PackagePath.empty())
		ImGui::TextWrapped("Package: %s", result.m_PackagePath.string().c_str());
	if (!result.m_Warnings.empty())
	{
		for (const std::string& warning : result.m_Warnings)
			ImGui::TextColored(ImVec4(0.92f, 0.70f, 0.36f, 1.0f), "%s", warning.c_str());
	}

	const auto sameLineIfFits = [](float nextWidth)
	{
		if (ImGui::GetContentRegionAvail().x > nextWidth + ImGui::GetStyle().ItemSpacing.x)
			ImGui::SameLine();
	};

	if (ImGui::Button("Run", ImVec2(92.0f, 0.0f)))
		m_ExportManager->RunLastExport();
	sameLineIfFits(118.0f);
	if (ImGui::Button("Open Folder", ImVec2(118.0f, 0.0f)))
		m_ExportManager->OpenLastOutputFolder();
	sameLineIfFits(92.0f);
	if (ImGui::Button("Rebuild", ImVec2(92.0f, 0.0f)))
	{
		m_LastBuildLogPreview.clear();
		m_ExportManager->RebuildLastExport();
	}
	sameLineIfFits(72.0f);
	if (ImGui::Button("Log", ImVec2(72.0f, 0.0f)))
	{
		m_LastBuildLogPreview = ReadPreviewText(result.m_BuildLogPath);
		m_ExportManager->OpenLastBuildLog();
	}
	if (!result.m_PackagePath.empty())
	{
		sameLineIfFits(92.0f);
		if (ImGui::Button("Package", ImVec2(92.0f, 0.0f)))
			m_ExportManager->OpenLastPackage();
	}
	sameLineIfFits(94.0f);
	if (ImGui::Button("Copy Log", ImVec2(94.0f, 0.0f)))
	{
		m_LastBuildLogPreview = ReadPreviewText(result.m_BuildLogPath);
		if (!m_LastBuildLogPreview.empty())
			ImGui::SetClipboardText(m_LastBuildLogPreview.c_str());
	}
	sameLineIfFits(82.0f);
	if (ImGui::Button("Clean", ImVec2(82.0f, 0.0f)))
	{
		m_ExportManager->CleanLastExportOutput();
		m_LastBuildLogPreview.clear();
	}
}

void ExportPanel::DrawBuildLogPreview()
{
	if (!m_ExportManager || !m_ExportManager->HasLastExport())
		return;

	const EditorExportResult& result = m_ExportManager->GetLastResult();
	if (result.m_BuildLogPath.empty())
		return;

	if (m_LastBuildLogPreview.empty())
		m_LastBuildLogPreview = ReadPreviewText(result.m_BuildLogPath);
	if (m_LastBuildLogPreview.empty())
		return;

	ImGui::Spacing();
	ImGui::SeparatorText("Build Log");
	ImGui::InputTextMultiline(
		"##ExportBuildLogPreview",
		m_LastBuildLogPreview.data(),
		m_LastBuildLogPreview.size() + 1,
		ImVec2(-1.0f, 130.0f),
		ImGuiInputTextFlags_ReadOnly);
}

_WHIP_END
