#pragma once

#include <Whip.h>

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "EditorManagerBase.h"

_WHIP_START

struct EditorExportSettings
{
	std::string m_ProductName;
	std::filesystem::path m_OutputRoot;
	bool m_CleanOutputDirectory = true;
	bool m_BuildScripts = true;
	bool m_RunAfterExport = false;
	bool m_OpenFolderAfterExport = false;
};

struct EditorExportResult
{
	std::filesystem::path m_OutputDirectory;
	std::filesystem::path m_ExecutablePath;
	std::filesystem::path m_ProjectPath;
	std::filesystem::path m_ManifestPath;
	std::vector<std::string> m_Warnings;
	bool m_ScriptBuildSucceeded = true;
};

class EditorExportManager : public EditorManagerBase // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	explicit EditorExportManager(EditorLayer* boundedLayer = nullptr);
	~EditorExportManager() override;

	bool BeginExport(EditorExportSettings settings);
	void UpdateAsyncOperations();
	void DrawAsyncProgressOverlay();
	void CancelExport(bool waitForCompletion = false);

	bool IsExportRunning() const;
	bool HasLastExport() const;
	bool OpenLastOutputFolder() const;
	bool RunLastExport() const;

	EditorExportSettings MakeDefaultSettings() const;
	const EditorExportResult& GetLastResult() const;
	const std::string& GetStatus() const;
	Async::JobProgressSnapshot GetProgressSnapshot() const;

private:
	bool PrepareForExport(EditorExportSettings& settings);
	void FinishExport(bool success, std::string status);

	Async::JobHandle m_ExportJob;
	std::shared_ptr<EditorExportResult> m_PendingResult;
	EditorExportResult m_LastResult;
	EditorExportSettings m_ActiveSettings;
	std::string m_Status = "Ready to export.";
	std::chrono::steady_clock::time_point m_ExportStartedAt{};
};

_WHIP_END
