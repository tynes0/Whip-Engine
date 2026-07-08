#pragma once

#include <Whip.h>

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "EditorManagerBase.h"

_WHIP_START

enum class EditorExportConfiguration : uint8_t
{
	Debug = 0,
	Release
};

struct EditorExportSettings
{
	std::string m_ProductName;
	std::string m_ProductVersion = "0.1.0";
	std::string m_CompanyName = "Whip";
	std::filesystem::path m_ProductIconPath;
	std::filesystem::path m_OutputRoot;
	EditorExportConfiguration m_Configuration = EditorExportConfiguration::Debug;
	bool m_BuildNativePlayer = true;
	bool m_CleanOutputDirectory = true;
	bool m_BuildScripts = true;
	bool m_RunProjectHealthCheck = true;
	bool m_BlockOnValidationErrors = true;
	bool m_PackageZip = true;
	bool m_RunAfterExport = false;
	bool m_OpenFolderAfterExport = false;
};

struct EditorExportResult
{
	std::filesystem::path m_OutputDirectory;
	std::filesystem::path m_ExecutablePath;
	std::filesystem::path m_ProjectPath;
	std::filesystem::path m_ManifestPath;
	std::filesystem::path m_BuildLogPath;
	std::filesystem::path m_PackagePath;
	std::vector<std::string> m_Warnings;
	EditorExportConfiguration m_Configuration = EditorExportConfiguration::Debug;
	int m_ValidationErrors = 0;
	int m_ValidationWarnings = 0;
	bool m_NativeBuildSucceeded = true;
	bool m_ScriptBuildSucceeded = true;
	bool m_PackageCreated = false;
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
	bool OpenLastBuildLog() const;
	bool OpenLastPackage() const;
	bool CleanLastExportOutput();
	bool RebuildLastExport();

	static const char* GetConfigurationName(EditorExportConfiguration configuration);
	static const char* GetBuildPresetName(EditorExportConfiguration configuration);

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
	std::vector<std::string> m_PreflightWarnings;
	std::string m_Status = "Ready to export.";
	std::chrono::steady_clock::time_point m_ExportStartedAt{};
};

_WHIP_END
