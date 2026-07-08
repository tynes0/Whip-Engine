#include <WhipPch.h>

#include <Whip-Editor/Managers/EditorExportManager.h>

#include <Whip-Editor/EditorLayer.h>
#include <Whip-Editor/Managers/EditorSceneManager.h>
#include <Whip-Editor/Managers/EditorScriptManager.h>

#include <Whip/Project/PlayerConfig.h>
#include <Whip/Project/ProjectSerializer.h>
#include <Whip/Utils/FileExtensions.h>
#include <Whip/Utils/PlatformUtils.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <fstream>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <string_view>

#include <imgui.h>

_WHIP_START

namespace
{
	constexpr const char* PlayerExecutableName = "Whip-Player.exe";
	constexpr const char* PlayerConfigFilename = "WhipPlayer.yaml";

	std::string TrimCopy(std::string value)
	{
		while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
			value.erase(value.begin());
		while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
			value.pop_back();
		return value;
	}

	std::string SanitizePathToken(std::string value, std::string_view fallback)
	{
		value = TrimCopy(std::move(value));
		for (char& character : value)
		{
			const unsigned char c = static_cast<unsigned char>(character);
			if (std::isalnum(c) || character == '_' || character == '-' || character == ' ')
				continue;
			character = '_';
		}

		value = TrimCopy(std::move(value));
		if (value.empty())
			value = std::string(fallback);
		for (char& character : value)
			if (character == ' ')
				character = '_';
		return value;
	}

	std::filesystem::path NormalizeAbsolutePath(const std::filesystem::path& path)
	{
		if (path.empty())
			return {};

		std::error_code error;
		std::filesystem::path absolutePath = std::filesystem::absolute(path, error);
		if (error)
			absolutePath = path;

		return absolutePath.lexically_normal();
	}

	bool PathsEqual(const std::filesystem::path& left, const std::filesystem::path& right)
	{
		return NormalizeAbsolutePath(left) == NormalizeAbsolutePath(right);
	}

	bool IsPathInside(const std::filesystem::path& candidate, const std::filesystem::path& root)
	{
		const std::filesystem::path normalizedCandidate = NormalizeAbsolutePath(candidate);
		const std::filesystem::path normalizedRoot = NormalizeAbsolutePath(root);
		if (normalizedCandidate.empty() || normalizedRoot.empty())
			return false;
		if (normalizedCandidate == normalizedRoot)
			return true;

		auto candidateIt = normalizedCandidate.begin();
		auto rootIt = normalizedRoot.begin();
		for (; rootIt != normalizedRoot.end(); ++rootIt, ++candidateIt)
		{
			if (candidateIt == normalizedCandidate.end() || *candidateIt != *rootIt)
				return false;
		}
		return true;
	}

	std::filesystem::path ResolvePlayerRuntimeDirectory()
	{
		const std::filesystem::path executableDirectory = Utils::GetExecutableDirectory();
		const std::array<std::filesystem::path, 4> candidates =
		{
			executableDirectory.parent_path() / "Whip-Player",
			executableDirectory / "Whip-Player",
			std::filesystem::current_path() / "Whip-Player",
			std::filesystem::current_path()
		};

		std::error_code error;
		for (const std::filesystem::path& candidate : candidates)
		{
			const std::filesystem::path playerExecutable = candidate / PlayerExecutableName;
			if (std::filesystem::exists(playerExecutable, error))
				return NormalizeAbsolutePath(candidate);
			error.clear();
		}

		return {};
	}

	bool ShouldSkipProjectAssetPath(const std::filesystem::path& relativePath)
	{
		std::string value = relativePath.generic_string();
		std::ranges::transform(value, value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return value.starts_with("scripts/intermediates/") ||
			value == "scripts/intermediates" ||
			value.starts_with("scripts/source/") ||
			value == "scripts/source" ||
			value.starts_with("scripts/.vs/") ||
			value == "scripts/.vs" ||
			value.starts_with("scripts/.idea/") ||
			value == "scripts/.idea";
	}

	size_t CountFiles(
		const std::filesystem::path& sourceDirectory,
		const std::function<bool(const std::filesystem::path&)>& skipRelativePath = {})
	{
		if (sourceDirectory.empty())
			return 0;

		std::error_code error;
		if (!std::filesystem::exists(sourceDirectory, error))
			return 0;

		size_t count = 0;
		for (std::filesystem::recursive_directory_iterator it(sourceDirectory, error), end; it != end; it.increment(error))
		{
			if (error)
			{
				error.clear();
				continue;
			}

			const std::filesystem::path relativePath = std::filesystem::relative(it->path(), sourceDirectory, error);
			if (error)
			{
				error.clear();
				continue;
			}
			if (skipRelativePath && skipRelativePath(relativePath))
			{
				if (it->is_directory(error))
					it.disable_recursion_pending();
				error.clear();
				continue;
			}
			if (it->is_regular_file(error))
				++count;
			error.clear();
		}
		return count;
	}

	void CopyDirectoryContents(
		const std::filesystem::path& sourceDirectory,
		const std::filesystem::path& destinationDirectory,
		Async::JobContext& context,
		float progressStart,
		float progressEnd,
		std::string_view label,
		const std::function<bool(const std::filesystem::path&)>& skipRelativePath = {})
	{
		std::error_code error;
		if (!std::filesystem::exists(sourceDirectory, error))
			throw std::runtime_error(std::string(label) + " source directory is missing: " + sourceDirectory.string());

		const size_t totalFiles = std::max<size_t>(1, CountFiles(sourceDirectory, skipRelativePath));
		size_t copiedFiles = 0;
		std::filesystem::create_directories(destinationDirectory, error);
		if (error)
			throw std::runtime_error("Could not create destination directory: " + destinationDirectory.string() + " (" + error.message() + ")");

		for (std::filesystem::recursive_directory_iterator it(sourceDirectory, error), end; it != end; it.increment(error))
		{
			if (context.IsCancellationRequested())
				return;

			if (error)
			{
				error.clear();
				continue;
			}

			const std::filesystem::path relativePath = std::filesystem::relative(it->path(), sourceDirectory, error);
			if (error)
			{
				error.clear();
				continue;
			}

			if (skipRelativePath && skipRelativePath(relativePath))
			{
				if (it->is_directory(error))
					it.disable_recursion_pending();
				error.clear();
				continue;
			}

			const std::filesystem::path destinationPath = destinationDirectory / relativePath;
			if (it->is_directory(error))
			{
				error.clear();
				std::filesystem::create_directories(destinationPath, error);
				if (error)
					throw std::runtime_error("Could not create directory: " + destinationPath.string() + " (" + error.message() + ")");
				continue;
			}

			if (!it->is_regular_file(error))
			{
				error.clear();
				continue;
			}

			error.clear();
			std::filesystem::create_directories(destinationPath.parent_path(), error);
			if (error)
				throw std::runtime_error("Could not create directory: " + destinationPath.parent_path().string() + " (" + error.message() + ")");

			error.clear();
			std::filesystem::copy_file(it->path(), destinationPath, std::filesystem::copy_options::overwrite_existing, error);
			if (error)
				throw std::runtime_error("Could not copy file: " + it->path().string() + " -> " + destinationPath.string() + " (" + error.message() + ")");

			++copiedFiles;
			const float alpha = static_cast<float>(copiedFiles) / static_cast<float>(totalFiles);
			context.SetProgress(std::lerp(progressStart, progressEnd, alpha), std::string(label) + " (" + std::to_string(copiedFiles) + "/" + std::to_string(totalFiles) + ")");
		}
	}

	void WriteExportManifest(
		const std::filesystem::path& manifestPath,
		const EditorExportSettings& settings,
		const EditorExportResult& result,
		Project& project,
		const std::filesystem::path& runtimeSourceDirectory)
	{
		std::ofstream stream(manifestPath, std::ios::binary | std::ios::trunc);
		if (!stream)
			throw std::runtime_error("Could not write export manifest: " + manifestPath.string());

		stream << "WhipExport:\n";
		stream << "  product: " << settings.m_ProductName << "\n";
		stream << "  platform: Windows-x86_64\n";
		stream << "  project_source: " << project.GetProjectPath().generic_string() << "\n";
		stream << "  exported_project: " << result.m_ProjectPath.filename().generic_string() << "\n";
		stream << "  executable: " << result.m_ExecutablePath.filename().generic_string() << "\n";
		stream << "  runtime_source: " << runtimeSourceDirectory.generic_string() << "\n";
		stream << "  start_scene: " << static_cast<uint64_t>(project.GetConfig().m_StartScene) << "\n";
		stream << "  scripts_built: " << (result.m_ScriptBuildSucceeded ? "true" : "false") << "\n";
	}

	struct ExportJobInput
	{
		Ref<Project> m_Project;
		EditorExportSettings m_Settings;
		std::filesystem::path m_RuntimeSourceDirectory;
		std::filesystem::path m_OutputDirectory;
		std::filesystem::path m_ProjectOutputDirectory;
		std::filesystem::path m_ProjectOutputPath;
		std::filesystem::path m_PlayerConfigPath;
		std::filesystem::path m_ProductExecutablePath;
	};

	void RunExportJob(const ExportJobInput& input, EditorExportResult& result, Async::JobContext& context)
	{
		std::error_code error;
		if (!input.m_Project)
			throw std::runtime_error("No project is loaded.");

		context.SetProgress(0.03f, "Preparing output directory");
		if (input.m_Settings.m_CleanOutputDirectory && std::filesystem::exists(input.m_OutputDirectory, error))
		{
			error.clear();
			std::filesystem::remove_all(input.m_OutputDirectory, error);
			if (error)
				throw std::runtime_error("Could not clean output directory: " + input.m_OutputDirectory.string() + " (" + error.message() + ")");
		}

		std::filesystem::create_directories(input.m_OutputDirectory, error);
		if (error)
			throw std::runtime_error("Could not create output directory: " + input.m_OutputDirectory.string() + " (" + error.message() + ")");

		if (input.m_Settings.m_BuildScripts)
		{
			context.SetProgress(0.08f, "Building scripts");
			result.m_ScriptBuildSucceeded = EditorScriptManager::BuildProjectScriptsForProject(input.m_Project,
				[&context](const std::string& message, bool, bool)
				{
					context.SetMessage(message);
				});
			if (!result.m_ScriptBuildSucceeded)
				result.m_Warnings.emplace_back("Script build failed. Existing script binaries were packaged.");
		}

		if (context.IsCancellationRequested())
			return;

		CopyDirectoryContents(input.m_RuntimeSourceDirectory, input.m_OutputDirectory, context, 0.18f, 0.48f, "Copying Whip Player runtime");

		context.SetProgress(0.50f, "Preparing exported project");
		std::filesystem::create_directories(input.m_ProjectOutputDirectory, error);
		if (error)
			throw std::runtime_error("Could not create exported project directory: " + input.m_ProjectOutputDirectory.string() + " (" + error.message() + ")");

		const std::filesystem::path sourceProjectPath = input.m_Project->GetProjectPath();
		std::filesystem::copy_file(sourceProjectPath, input.m_ProjectOutputPath, std::filesystem::copy_options::overwrite_existing, error);
		if (error)
			throw std::runtime_error("Could not copy project file: " + sourceProjectPath.string() + " (" + error.message() + ")");

		CopyDirectoryContents(
			input.m_Project->GetAssetDirectory(),
			input.m_ProjectOutputDirectory / input.m_Project->GetConfig().m_AssetDirectory,
			context,
			0.54f,
			0.86f,
			"Copying project assets",
			ShouldSkipProjectAssetPath);

		context.SetProgress(0.88f, "Writing player config");
		PlayerConfig playerConfig;
		playerConfig.m_ProjectPath = std::filesystem::relative(input.m_ProjectOutputPath, input.m_OutputDirectory, error);
		if (error)
			playerConfig.m_ProjectPath = input.m_ProjectOutputPath.filename();
		playerConfig.m_WindowTitle = input.m_Settings.m_ProductName;
		PlayerConfigSerializer playerConfigSerializer(playerConfig);
		if (!playerConfigSerializer.Serialize(input.m_PlayerConfigPath))
			throw std::runtime_error("Could not write WhipPlayer.yaml.");

		const std::filesystem::path sourceExecutable = input.m_OutputDirectory / PlayerExecutableName;
		if (!std::filesystem::exists(sourceExecutable, error))
			throw std::runtime_error("Whip-Player.exe was not found in copied runtime.");

		error.clear();
		if (!PathsEqual(sourceExecutable, input.m_ProductExecutablePath))
		{
			std::filesystem::copy_file(sourceExecutable, input.m_ProductExecutablePath, std::filesystem::copy_options::overwrite_existing, error);
			if (error)
				throw std::runtime_error("Could not create product executable: " + input.m_ProductExecutablePath.string() + " (" + error.message() + ")");

			error.clear();
			std::filesystem::remove(sourceExecutable, error);
		}

		context.SetProgress(0.95f, "Writing export manifest");
		result.m_OutputDirectory = input.m_OutputDirectory;
		result.m_ExecutablePath = input.m_ProductExecutablePath;
		result.m_ProjectPath = input.m_ProjectOutputPath;
		result.m_ManifestPath = input.m_OutputDirectory / "WhipExport.yaml";
		WriteExportManifest(result.m_ManifestPath, input.m_Settings, result, *input.m_Project, input.m_RuntimeSourceDirectory);

		context.SetProgress(1.0f, "Export complete");
	}
}

EditorExportManager::EditorExportManager(EditorLayer* boundedLayer)
	: EditorManagerBase(boundedLayer)
{
}

EditorExportManager::~EditorExportManager() = default;

EditorExportSettings EditorExportManager::MakeDefaultSettings() const
{
	EditorExportSettings settings;
	if (Ref<Project> project = Project::GetActive())
	{
		settings.m_ProductName = SanitizePathToken(project->GetConfig().m_Name, project->GetProjectPath().stem().string());
		settings.m_OutputRoot = project->GetProjectDirectory() / "Builds" / "Windows";
	}
	else
	{
		settings.m_ProductName = "WhipGame";
		settings.m_OutputRoot = std::filesystem::current_path() / "Builds" / "Windows";
	}
	return settings;
}

bool EditorExportManager::BeginExport(EditorExportSettings settings)
{
	WHP_PROFILE_FUNCTION();
	if (IsExportRunning())
	{
		m_Status = "Export is already running.";
		return false;
	}

	if (!PrepareForExport(settings))
		return false;

	const std::string productName = SanitizePathToken(settings.m_ProductName, "WhipGame");
	settings.m_ProductName = productName;

	const std::filesystem::path runtimeSourceDirectory = ResolvePlayerRuntimeDirectory();
	if (runtimeSourceDirectory.empty())
	{
		m_Status = "Whip-Player build output was not found. Build the Whip-Player target first.";
		WHP_EDITOR_ERROR("[Export] Whip-Player build output was not found.");
		return false;
	}

	Ref<Project> project = Project::GetActive();
	const std::filesystem::path outputDirectory = NormalizeAbsolutePath(settings.m_OutputRoot / productName);
	const std::filesystem::path projectOutputDirectory = outputDirectory / "Project";
	const std::filesystem::path projectOutputPath = projectOutputDirectory / (productName + FileExtensions::Project);
	const std::filesystem::path productExecutablePath = outputDirectory / (productName + ".exe");

	if (PathsEqual(outputDirectory, project->GetProjectDirectory()) || IsPathInside(outputDirectory, project->GetAssetDirectory()))
	{
		m_Status = "Output directory cannot be the project folder or inside Assets.";
		WHP_EDITOR_ERROR("[Export] Refusing unsafe output directory: {0}", outputDirectory.string());
		return false;
	}

	if (IsPathInside(outputDirectory, runtimeSourceDirectory))
	{
		m_Status = "Output directory cannot be inside the Whip-Player runtime folder.";
		WHP_EDITOR_ERROR("[Export] Refusing output inside runtime source directory: {0}", outputDirectory.string());
		return false;
	}

	auto result = std::make_shared<EditorExportResult>();
	ExportJobInput input;
	input.m_Project = project;
	input.m_Settings = settings;
	input.m_RuntimeSourceDirectory = runtimeSourceDirectory;
	input.m_OutputDirectory = outputDirectory;
	input.m_ProjectOutputDirectory = projectOutputDirectory;
	input.m_ProjectOutputPath = projectOutputPath;
	input.m_PlayerConfigPath = outputDirectory / PlayerConfigFilename;
	input.m_ProductExecutablePath = productExecutablePath;

	m_ActiveSettings = settings;
	m_PendingResult = result;
	m_ExportStartedAt = std::chrono::steady_clock::now();
	m_Status = "Exporting...";
	m_ExportJob = Async::JobSystem::Get().Submit("Export Windows Player", [input = std::move(input), result](Async::JobContext& context)
	{
		RunExportJob(input, *result, context);
	});
	return true;
}

bool EditorExportManager::PrepareForExport(EditorExportSettings& settings)
{
	WHP_PROFILE_FUNCTION();
	EditorLayer& layer = GetLayer();
	if (!Project::GetActive() || !Project::Loaded())
	{
		m_Status = "No project is loaded.";
		return false;
	}

	if (settings.m_OutputRoot.empty())
	{
		m_Status = "Choose an output folder.";
		return false;
	}

	if (settings.m_ProductName.empty())
		settings.m_ProductName = Project::GetActive()->GetConfig().m_Name;

	if (layer.m_SceneManager.State() != EditorSceneState::Edit)
		layer.m_SceneManager.OnSceneStop();

	if (layer.m_SceneManager.IsSceneDirty() && !layer.m_SceneManager.EditorScenePath().empty())
		layer.m_SceneManager.SaveScene();

	if (!Project::SaveActive())
	{
		m_Status = "Project file could not be saved.";
		return false;
	}

	Ref<Project> project = Project::GetActive();
	if (!project->GetEditorAssetManager())
	{
		m_Status = "Asset registry is not available.";
		return false;
	}

	bool registrySaved = false;
	project->GetEditorAssetManager()->SerializeAssetRegistry(&registrySaved);
	if (!registrySaved)
	{
		m_Status = "Asset registry could not be saved.";
		return false;
	}

	const AssetHandle startScene = project->GetConfig().m_StartScene;
	if (startScene == 0 || !project->GetEditorAssetManager()->IsAssetHandleValid(startScene) ||
		project->GetEditorAssetManager()->GetAssetType(startScene) != AssetType::Scene)
	{
		m_Status = "Set a valid start scene before exporting.";
		WHP_EDITOR_WARN("[Export] Project has no valid start scene.");
		return false;
	}

	return true;
}

void EditorExportManager::UpdateAsyncOperations()
{
	WHP_PROFILE_FUNCTION();
	if (!IsExportRunning() || !m_ExportJob.IsDone())
		return;

	const Async::JobProgressSnapshot snapshot = m_ExportJob.Snapshot();
	if (snapshot.m_Status == Async::JobStatus::Succeeded && m_PendingResult)
	{
		m_LastResult = *m_PendingResult;
		if (m_ActiveSettings.m_OpenFolderAfterExport)
			Utils::OpenExternalPath(m_LastResult.m_OutputDirectory);
		if (m_ActiveSettings.m_RunAfterExport)
			Utils::OpenExternalPath(m_LastResult.m_ExecutablePath);

		FinishExport(true, m_LastResult.m_ScriptBuildSucceeded ? "Export complete." : "Export complete, script build failed.");
		return;
	}

	if (snapshot.m_Status == Async::JobStatus::Cancelled)
	{
		FinishExport(false, "Export cancelled.");
		return;
	}

	const std::string error = !snapshot.m_Error.empty() ? snapshot.m_Error : "Export failed.";
	WHP_EDITOR_ERROR("[Export] {0}", error);
	FinishExport(false, error);
}

void EditorExportManager::FinishExport(bool success, std::string status)
{
	WHP_PROFILE_FUNCTION();
	if (success)
	{
		WHP_EDITOR_INFO("[Export] Windows player exported: {0}", m_LastResult.m_OutputDirectory.string());
		for (const std::string& warning : m_LastResult.m_Warnings)
			WHP_EDITOR_WARN("[Export] {0}", warning);
	}

	m_ExportJob = Async::JobHandle();
	m_PendingResult.reset();
	m_Status = std::move(status);
}

void EditorExportManager::CancelExport(bool waitForCompletion)
{
	if (!m_ExportJob.IsValid())
		return;

	m_ExportJob.Cancel();
	m_Status = "Cancelling export...";
	if (!waitForCompletion)
		return;

	m_ExportJob.Wait();

	m_ExportJob = Async::JobHandle();
	m_PendingResult.reset();
	m_Status = "Export cancelled.";
}

bool EditorExportManager::IsExportRunning() const
{
	return m_ExportJob.IsValid();
}

bool EditorExportManager::HasLastExport() const
{
	return !m_LastResult.m_OutputDirectory.empty() && !m_LastResult.m_ExecutablePath.empty();
}

bool EditorExportManager::OpenLastOutputFolder() const
{
	return HasLastExport() && Utils::OpenExternalPath(m_LastResult.m_OutputDirectory);
}

bool EditorExportManager::RunLastExport() const
{
	return HasLastExport() && Utils::OpenExternalPath(m_LastResult.m_ExecutablePath);
}

const EditorExportResult& EditorExportManager::GetLastResult() const
{
	return m_LastResult;
}

const std::string& EditorExportManager::GetStatus() const
{
	return m_Status;
}

Async::JobProgressSnapshot EditorExportManager::GetProgressSnapshot() const
{
	return m_ExportJob.Snapshot();
}

void EditorExportManager::DrawAsyncProgressOverlay()
{
	if (!IsExportRunning())
		return;

	const Async::JobProgressSnapshot snapshot = m_ExportJob.Snapshot();
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	const ImVec2 overlaySize(560.0f, 178.0f);
	ImGui::SetNextWindowPos(
		ImVec2(viewport->WorkPos.x + (viewport->WorkSize.x - overlaySize.x) * 0.5f, viewport->WorkPos.y + viewport->WorkSize.y - overlaySize.y - 42.0f),
		ImGuiCond_Always);
	ImGui::SetNextWindowSize(overlaySize, ImGuiCond_Always);

	const ImGuiWindowFlags flags =
		ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 7.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18.0f, 16.0f));
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.035f, 0.050f, 0.060f, 0.97f));
	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.24f, 0.43f, 0.52f, 0.95f));
	ImGui::Begin("##ExportAsyncProgressOverlay", nullptr, flags);

	const ImVec2 windowPos = ImGui::GetWindowPos();
	const ImVec2 windowSize = ImGui::GetWindowSize();
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	drawList->AddRect(windowPos, ImVec2(windowPos.x + windowSize.x, windowPos.y + windowSize.y), IM_COL32(77, 132, 156, 220), 7.0f, 0, 1.2f);
	drawList->AddRectFilled(windowPos, ImVec2(windowPos.x + 4.0f, windowPos.y + windowSize.y), IM_COL32(95, 169, 202, 230), 7.0f, ImDrawFlags_RoundCornersLeft);

	ImGui::TextUnformatted("Windows Export");
	ImGui::SameLine();
	ImGui::TextDisabled("%s", snapshot.m_CancelRequested ? "Cancelling" : "Running");

	const std::string message = !snapshot.m_Message.empty() ? snapshot.m_Message : "Packaging player...";
	ImGui::Spacing();
	ImGui::TextWrapped("%s", message.c_str());

	if (m_PendingResult && !m_PendingResult->m_OutputDirectory.empty())
		ImGui::TextDisabled("%s", m_PendingResult->m_OutputDirectory.string().c_str());
	else
		ImGui::TextDisabled("%s", (m_ActiveSettings.m_OutputRoot / m_ActiveSettings.m_ProductName).string().c_str());

	ImGui::Spacing();
	const float progress = std::clamp(snapshot.m_Progress, 0.0f, 1.0f);
	ImGui::ProgressBar(progress, ImVec2(-1.0f, 11.0f), "");
	ImGui::SameLine(0.0f, 10.0f);
	ImGui::TextDisabled("%d%%", static_cast<int>(std::round(progress * 100.0f)));

	ImGui::SetCursorPosY(windowSize.y - 38.0f);
	ImGui::BeginDisabled(snapshot.m_CancelRequested);
	if (ImGui::Button("Cancel", ImVec2(96.0f, 26.0f)))
		CancelExport(false);
	ImGui::EndDisabled();

	ImGui::End();
	ImGui::PopStyleColor(2);
	ImGui::PopStyleVar(2);
}

_WHIP_END
