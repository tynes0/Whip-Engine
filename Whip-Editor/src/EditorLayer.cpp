#include "EditorLayer.h"

#include <Whip/Core/EntryPoint.h>
#include <Whip/Utils/FileExtensions.h>
#include <Whip/Scene/SceneSerializer.h>
#include <Whip/Scripting/ScriptEngine.h>
#include <Whip/Scripting/ScriptProjectGenerator.h>
#include <Whip/Utils/PlatformUtils.h>
#include <Whip/UI/UIHelpers.h>
#include <Whip/UI/UIProjectLoader.h>
#include <Whip/Math/Math.h>
#include <Whip/Asset/AssetManager.h>
#include <Whip/Asset/AssetUtils.h>
#include <Whip/Asset/SceneImporter.h>
#include <Whip/Asset/TextureAtlasParser.h>

#include "Helpers/IconManager.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_set>

#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <entt.hpp>
#include <ImGuizmo.h>
#include <yaml-cpp/yaml.h>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

_WHIP_START

namespace
{
	bool IsControlDown()
	{
		return Input::IsKeyDown(Key::LeftControl) || Input::IsKeyDown(Key::RightControl);
	}

	std::string LowerCopy(std::string value)
	{
		std::ranges::transform(value, value.begin(),
		                       [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
		return value;
	}

	bool IsIgnoredScriptWatchSegment(const std::filesystem::path& path)
	{
		for (const auto& segment : path)
		{
			const std::string name = LowerCopy(segment.string());
			if (name == "binaries" || name == "intermediates" || name == "obj" || name == "bin" ||
				name == ".vs" || name == ".idea" || name == "whip-scriptcore")
			{
				return true;
			}
		}
		return false;
	}

	bool IsScriptSourceWatchEvent(const std::filesystem::path& path, filewatch::Event eventType)
	{
		if (path.empty() || IsIgnoredScriptWatchSegment(path))
			return false;

		switch (eventType)
		{
		case filewatch::Event::added:
		case filewatch::Event::removed:
		case filewatch::Event::modified:
		case filewatch::Event::renamed_old:
		case filewatch::Event::renamed_new:
			break;
		}

		const std::string extension = LowerCopy(path.extension().string());
		return extension == ".cs" ||
			extension == ".csproj" ||
			extension == ".sln" ||
			extension == ".props" ||
			extension == ".targets" ||
			extension == ".config" ||
			extension == ".rsp";
	}

	std::string ScriptSourceEventName(filewatch::Event eventType)
	{
		switch (eventType)
		{
		case filewatch::Event::added: return "added";
		case filewatch::Event::removed: return "removed";
		case filewatch::Event::modified: return "modified";
		case filewatch::Event::renamed_old:
		case filewatch::Event::renamed_new: return "renamed";
		}
		return "changed";
	}

	int GizmoSnapIndex(int operation)
	{
		if (operation == ImGuizmo::OPERATION::TRANSLATE)
			return 0;
		if (operation == ImGuizmo::OPERATION::ROTATE)
			return 1;
		if (operation == ImGuizmo::OPERATION::SCALE || operation == ImGuizmo::OPERATION::SCALEU)
			return 2;
		return -1;
	}

	ImU32 ColorU32(float r, float g, float b, float a = 1.0f)
	{
		return ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, a));
	}

	constexpr UI::EditorShortcutAction CommandPaletteActions[] =
	{
		UI::EditorShortcutAction::OpenProject,
		UI::EditorShortcutAction::NewScene,
		UI::EditorShortcutAction::SaveScene,
		UI::EditorShortcutAction::SaveSceneAs,
		UI::EditorShortcutAction::SaveProject,
		UI::EditorShortcutAction::CloseScene,
		UI::EditorShortcutAction::Undo,
		UI::EditorShortcutAction::Redo,
		UI::EditorShortcutAction::SelectAll,
		UI::EditorShortcutAction::Copy,
		UI::EditorShortcutAction::Paste,
		UI::EditorShortcutAction::Cut,
		UI::EditorShortcutAction::DuplicateEntity,
		UI::EditorShortcutAction::DeleteEntity,
		UI::EditorShortcutAction::Play,
		UI::EditorShortcutAction::Simulate,
		UI::EditorShortcutAction::Stop,
		UI::EditorShortcutAction::Pause,
		UI::EditorShortcutAction::GizmoNone,
		UI::EditorShortcutAction::GizmoTranslate,
		UI::EditorShortcutAction::GizmoRotate,
		UI::EditorShortcutAction::GizmoScale,
		UI::EditorShortcutAction::ReloadScripts,
		UI::EditorShortcutAction::OpenSettings,
		UI::EditorShortcutAction::OpenCommandPalette
	};

	bool CommandMatchesFilter(UI::EditorShortcutAction action, const char* filter)
	{
		if (!filter || filter[0] == '\0')
			return true;

		std::string needle = LowerCopy(filter);
		std::string haystack = LowerCopy(std::string(UI::UISettings::GetActionDisplayName(action)) + " " + UI::UISettings::GetActionCategory(action));
		return haystack.find(needle) != std::string::npos;
	}

	std::string SanitizeProjectToken(std::string value, const std::string& fallback)
	{
		std::erase_if(value, [](unsigned char c)
		{
			return !std::isalnum(c) && c != '_' && c != '-' && c != ' ';
		});

		while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
			value.erase(value.begin());
		while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
			value.pop_back();

		if (value.empty())
			value = fallback;
		return value;
	}

	std::string SanitizePathToken(std::string value, const std::string& fallback)
	{
		value = SanitizeProjectToken(std::move(value), fallback);
		for (char& c : value)
			if (c == ' ')
				c = '_';
		return value;
	}

	std::string GetScriptWorkspaceName(const ProjectConfig& config)
	{
		const std::string fallback = SanitizePathToken(config.m_Name, "Untitled");
		if (!config.m_ScriptModulePath.empty())
		{
			const std::string moduleName = config.m_ScriptModulePath.stem().string();
			if (!moduleName.empty())
				return SanitizePathToken(moduleName, fallback);
		}

		return fallback;
	}

	void WriteVec3(YAML::Emitter& out, const glm::vec3& value)
	{
		out << YAML::Flow << YAML::BeginSeq << value.x << value.y << value.z << YAML::EndSeq;
	}

	glm::vec3 ReadVec3(const YAML::Node& node, const glm::vec3& fallback)
	{
		if (!node || !node.IsSequence() || node.size() != 3)
			return fallback;
		return { node[0].as<float>(fallback.x), node[1].as<float>(fallback.y), node[2].as<float>(fallback.z) };
	}

	UI::EditorTheme ThemeFromString(const std::string& value)
	{
		if (value == "Graphite")
			return UI::EditorTheme::Graphite;
		if (value == "Ember")
			return UI::EditorTheme::Ember;
		if (value == "Moss")
			return UI::EditorTheme::Moss;
		if (value == "Porcelain" || value == "Light")
			return UI::EditorTheme::Light;
		return UI::EditorTheme::WhipDark;
	}

	std::string ReadTextFile(const std::filesystem::path& path)
	{
		std::ifstream stream(path, std::ios::binary);
		if (!stream)
			return {};

		return std::string{ std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() };
	}

	bool WriteTextFile(const std::filesystem::path& path, const std::string& contents)
	{
		std::error_code error;
		std::filesystem::create_directories(path.parent_path(), error);
		std::ofstream stream(path, std::ios::binary | std::ios::trunc);
		if (!stream)
			return false;

		stream << contents;
		return true;
	}

	std::filesystem::path NormalizeProjectListPath(const std::filesystem::path& path)
	{
		if (path.empty())
			return {};

		std::error_code error;
		std::filesystem::path normalizedPath = std::filesystem::weakly_canonical(path, error);
		if (error)
		{
			error.clear();
			normalizedPath = std::filesystem::absolute(path, error);
		}

		return error ? path : normalizedPath.lexically_normal();
	}

	bool PathsMatchForRecentProject(const std::filesystem::path& left, const std::filesystem::path& right)
	{
		const std::filesystem::path normalizedLeft = NormalizeProjectListPath(left);
		const std::filesystem::path normalizedRight = NormalizeProjectListPath(right);
		return !normalizedLeft.empty() && normalizedLeft == normalizedRight;
	}

	bool PathIsOrIsUnder(const std::filesystem::path& path, const std::filesystem::path& directory)
	{
		const std::filesystem::path normalizedPath = path.lexically_normal();
		const std::filesystem::path normalizedDirectory = directory.lexically_normal();
		if (normalizedPath == normalizedDirectory)
			return true;

		auto pathIt = normalizedPath.begin();
		auto directoryIt = normalizedDirectory.begin();
		for (; directoryIt != normalizedDirectory.end(); ++directoryIt, ++pathIt)
		{
			if (pathIt == normalizedPath.end() || *pathIt != *directoryIt)
				return false;
		}

		return true;
	}

	bool CreateDirectoryChecked(const std::filesystem::path& path, std::string_view label)
	{
		std::error_code error;
		std::filesystem::create_directories(path, error);
		if (!error)
			return true;

		WHP_EDITOR_ERROR("[Whip Hub] Could not create {}: {} ({})", label, path.string(), error.message());
		return false;
	}

	std::filesystem::path MakeUniquePath(const std::filesystem::path& targetPath)
	{
		std::error_code error;
		if (!std::filesystem::exists(targetPath, error))
			return targetPath;

		const std::filesystem::path parent = targetPath.parent_path();
		const std::string stem = targetPath.stem().string();
		const std::string extension = targetPath.extension().string();
		for (uint32_t index = 1; index < 10000; ++index)
		{
			std::filesystem::path candidate = parent / std::format("{}_{}{}", stem, index, extension);
			error.clear();
			if (!std::filesystem::exists(candidate, error))
				return candidate;
		}

		return targetPath;
	}

	std::filesystem::path DefaultImportDirectoryForType(AssetType type)
	{
		switch (type)
		{
		case AssetType::Scene: return "Scenes";
		case AssetType::Texture2D: return "textures";
		case AssetType::Audio: return "Audios";
		case AssetType::Font: return "fonts";
		case AssetType::Animation: return "Animations";
		case AssetType::AnimationController: return "Animations";
		case AssetType::Entity: return "EntityTemplates";
		case AssetType::None: return {};
		}
		return {};
	}

	std::filesystem::path LocateScriptCoreSourceDirectory()
	{
		std::error_code error;
		for (std::filesystem::path probe = std::filesystem::current_path(); !probe.empty(); probe = probe.parent_path())
		{
			const std::array<std::filesystem::path, 2> candidates =
			{
				probe / "Resources" / "Scripts" / "Whip-ScriptCore" / "Source",
				probe / "Whip-ScriptCore" / "Source"
			};

			for (const auto& candidate : candidates)
			{
				if (std::filesystem::exists(candidate, error) && std::filesystem::is_directory(candidate, error))
					return candidate;
			}

			if (probe == probe.parent_path())
				break;
		}

		return {};
	}

	std::filesystem::path LocateScriptCoreBinary()
	{
		std::error_code error;
		for (std::filesystem::path probe = std::filesystem::current_path(); !probe.empty(); probe = probe.parent_path())
		{
			const std::array<std::filesystem::path, 5> candidates =
			{
				probe / "Resources" / "Scripts" / "Whip-ScriptCore.dll",
				probe / "Whip-ScriptCore.dll",
				probe / "bin" / "Debug-windows-x86_64" / "Whip-Editor" / "Resources" / "Scripts" / "Whip-ScriptCore.dll",
				probe / "bin" / "Release-windows-x86_64" / "Whip-Editor" / "Resources" / "Scripts" / "Whip-ScriptCore.dll",
				probe / "bin" / "Dist-windows-x86_64" / "Whip-Editor" / "Resources" / "Scripts" / "Whip-ScriptCore.dll"
			};

			for (const auto& candidate : candidates)
			{
				if (std::filesystem::exists(candidate, error) && std::filesystem::is_regular_file(candidate, error))
					return candidate;
			}

			if (probe == probe.parent_path())
				break;
		}

		return {};
	}

	bool SyncScriptCoreBinary(const std::filesystem::path& scriptsDirectory)
	{
		const std::filesystem::path source = LocateScriptCoreBinary();
		if (source.empty())
		{
			WHP_EDITOR_WARN("[Script Build] Could not find Whip-ScriptCore.dll to sync into the script workspace.");
			return false;
		}

		const std::filesystem::path destination = scriptsDirectory / "Binaries" / "Whip-ScriptCore.dll";
		std::error_code error;
		std::filesystem::create_directories(destination.parent_path(), error);
		if (error)
		{
			WHP_EDITOR_WARN(std::string("[Script Build] Could not create script binaries directory: ") + error.message());
			return false;
		}

		std::filesystem::copy_file(source, destination, std::filesystem::copy_options::overwrite_existing, error);
		if (error)
		{
			WHP_EDITOR_WARN(std::string("[Script Build] Could not copy Whip-ScriptCore.dll: ") + error.message());
			return false;
		}

		const std::filesystem::path sourcePdb = source.parent_path() / "Whip-ScriptCore.pdb";
		if (std::filesystem::exists(sourcePdb, error))
		{
			error.clear();
			std::filesystem::copy_file(sourcePdb, destination.parent_path() / "Whip-ScriptCore.pdb", std::filesystem::copy_options::overwrite_existing, error);
		}
		return true;
	}

	bool CopyDirectoryRecursive(const std::filesystem::path& source, const std::filesystem::path& destination)
	{
		std::error_code error;
		if (!std::filesystem::exists(source, error))
			return false;

		std::filesystem::create_directories(destination, error);
		if (error)
			return false;

		for (const auto& entry : std::filesystem::recursive_directory_iterator(source, error))
		{
			if (error)
				return false;

			const std::filesystem::path relative = std::filesystem::relative(entry.path(), source, error);
			if (error)
				return false;

			const std::filesystem::path target = destination / relative;
			if (entry.is_directory(error))
			{
				std::filesystem::create_directories(target, error);
			}
			else if (entry.is_regular_file(error))
			{
				std::filesystem::create_directories(target.parent_path(), error);
				if (!error)
					std::filesystem::copy_file(entry.path(), target, std::filesystem::copy_options::overwrite_existing, error);
			}

			if (error)
				return false;
		}

		return true;
	}

	bool RefreshScriptWorkspaceFiles(const std::filesystem::path& scriptsDirectory, const std::string& projectFolderName);

	bool WriteScriptProjectFiles(const std::filesystem::path& projectDirectory, const std::string& projectFolderName)
	{
		const std::filesystem::path scriptsDirectory = projectDirectory / "Assets" / "Scripts";
		if (!RefreshScriptWorkspaceFiles(scriptsDirectory, projectFolderName))
			return false;

		if (!WriteTextFile(scriptsDirectory / "Source" / "StarterEntity.cs", ScriptProjectGenerator::MakeStarterScript(projectFolderName)))
		{
			WHP_EDITOR_ERROR("[Whip Hub] Could not write starter script file.");
			return false;
		}

		SyncScriptCoreBinary(scriptsDirectory);
		return true;
	}

bool RefreshScriptWorkspaceFiles(const std::filesystem::path& scriptsDirectory, const std::string& projectFolderName)
{
	const std::string projectGuid = ScriptProjectGenerator::MakeStableGuid(projectFolderName + ":scripts");
	const std::string coreGuid = ScriptProjectGenerator::MakeStableGuid(projectFolderName + ":scriptcore");

	const ScriptProjectGenerator::CSharpProjectGenerationSettings csharpSettings{};

	ScriptProjectGenerator::SolutionGenerationSettings solutionSettings{};
	solutionSettings.m_Projects =
	{
		ScriptProjectGenerator::SolutionProjectEntry {
			.m_Name = projectFolderName,
			.m_RelativePath = projectFolderName + ".csproj",
			.m_Guid = projectGuid
		},
		ScriptProjectGenerator::SolutionProjectEntry {
			.m_Name = csharpSettings.m_ScriptCoreProjectName,
			.m_RelativePath = csharpSettings.m_ScriptCoreProjectRelativePath,
			.m_Guid = coreGuid
		}
	};

	const std::filesystem::path scriptCoreSource = LocateScriptCoreSourceDirectory();
	if (scriptCoreSource.empty() || !CopyDirectoryRecursive(scriptCoreSource, scriptsDirectory / "Whip-ScriptCore" / "Source"))
	{
		WHP_EDITOR_ERROR("[Whip Hub] Could not copy Whip-ScriptCore SDK sources.");
		return false;
	}

	if (!WriteTextFile(scriptsDirectory / "Whip-ScriptCore" / "Whip-ScriptCore.csproj", ScriptProjectGenerator::MakeScriptCoreCsproj(csharpSettings)))
	{
		WHP_EDITOR_ERROR("[Whip Hub] Could not write Whip-ScriptCore Project file.");
		return false;
	}

	if (!WriteTextFile(scriptsDirectory / "Directory.Build.props", ScriptProjectGenerator::MakeDirectoryBuildProps(projectFolderName, csharpSettings)))
	{
		WHP_EDITOR_ERROR("[Whip Hub] Could not write script build properties file.");
		return false;
	}

	if (!WriteTextFile(scriptsDirectory / (projectFolderName + ".csproj"), ScriptProjectGenerator::MakeProjectCsproj(projectFolderName, coreGuid, csharpSettings)))
	{
		WHP_EDITOR_ERROR("[Whip Hub] Could not write script Project file.");
		return false;
	}

	if (!WriteTextFile(scriptsDirectory / (projectFolderName + ".sln"), ScriptProjectGenerator::MakeScriptSolution(solutionSettings)))
	{
		WHP_EDITOR_ERROR("[Whip Hub] Could not write script solution file.");
		return false;
	}

	return true;
}

	std::filesystem::path FindScriptProjectFile(const std::filesystem::path& scriptsDirectory, const std::string& scriptWorkspaceName)
	{
		std::filesystem::path preferred = scriptsDirectory / (SanitizePathToken(scriptWorkspaceName, "Untitled") + ".sln");
		std::error_code error;
		if (std::filesystem::exists(preferred, error))
			return preferred;

		preferred = scriptsDirectory / (SanitizePathToken(scriptWorkspaceName, "Untitled") + ".csproj");
		if (std::filesystem::exists(preferred, error))
			return preferred;

		if (!std::filesystem::exists(scriptsDirectory, error))
			return {};

		for (const auto& entry : std::filesystem::directory_iterator(scriptsDirectory, error))
		{
			if (error)
				break;
			if (entry.is_regular_file(error) && entry.path().extension() == ".sln")
				return entry.path();
		}

		for (const auto& entry : std::filesystem::directory_iterator(scriptsDirectory, error))
		{
			if (error)
				break;
			if (entry.is_regular_file(error) && entry.path().extension() == ".csproj")
				return entry.path();
		}

		return {};
	}

	std::string QuoteCommandPath(const std::filesystem::path& path)
	{
		return std::format("\"{}\"", path.string());
	}

#ifdef _WIN32
	int RunWindowsProcess(const std::string& command, bool logOutput, std::string* firstLine = nullptr)
	{
		SECURITY_ATTRIBUTES securityAttributes{};
		securityAttributes.nLength = sizeof(SECURITY_ATTRIBUTES);
		securityAttributes.bInheritHandle = TRUE;

		HANDLE readPipe = nullptr;
		HANDLE writePipe = nullptr;
		if (!CreatePipe(&readPipe, &writePipe, &securityAttributes, 0))
		{
			WHP_EDITOR_ERROR("[Script Build] Could not create process output pipe.");
			return -1;
		}
		SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

		STARTUPINFOA startupInfo{};
		startupInfo.cb = sizeof(STARTUPINFOA);
		startupInfo.dwFlags = STARTF_USESTDHANDLES;
		startupInfo.hStdOutput = writePipe;
		startupInfo.hStdError = writePipe;
		startupInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

		PROCESS_INFORMATION processInfo{};
		std::string mutableCommand = command;
		BOOL created = CreateProcessA(nullptr, mutableCommand.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &startupInfo, &processInfo);
		CloseHandle(writePipe);

		if (!created)
		{
			const DWORD error = GetLastError();
			CloseHandle(readPipe);
			WHP_EDITOR_ERROR(std::string("[Script Build] Could not start process. Windows error ") + std::to_string(error) + ".");
			return -1;
		}

		std::string pending;
		std::array<char, 512> buffer{};
		DWORD bytesRead = 0;
		while (ReadFile(readPipe, buffer.data(), static_cast<DWORD>(buffer.size() - 1), &bytesRead, nullptr) && bytesRead > 0)
		{
			buffer[bytesRead] = '\0';
			pending.append(buffer.data(), bytesRead);

			size_t newline;
			while ((newline = pending.find('\n')) != std::string::npos)
			{
				std::string line = pending.substr(0, newline);
				pending.erase(0, newline + 1);
				if (!line.empty() && line.back() == '\r')
					line.pop_back();

				if (firstLine && firstLine->empty() && !line.empty())
					*firstLine = line;
				if (logOutput && !line.empty())
					WHP_EDITOR_INFO(std::string("[Script Build] ") + line);
			}
		}

		if (!pending.empty())
		{
			if (!pending.empty() && pending.back() == '\r')
				pending.pop_back();
			if (firstLine && firstLine->empty() && !pending.empty())
				*firstLine = pending;
			if (logOutput && !pending.empty())
				WHP_EDITOR_INFO(std::string("[Script Build] ") + pending);
		}

		WaitForSingleObject(processInfo.hProcess, INFINITE);
		DWORD exitCode = 0;
		GetExitCodeProcess(processInfo.hProcess, &exitCode);

		CloseHandle(processInfo.hProcess);
		CloseHandle(processInfo.hThread);
		CloseHandle(readPipe);
		return static_cast<int>(exitCode);
	}
#endif

	std::filesystem::path PathFromEnvironment(const char* name)
	{
		const char* value = std::getenv(name); // NOLINT(concurrency-mt-unsafe)
		return value ? std::filesystem::path(value) : std::filesystem::path{};
	}

	std::filesystem::path FindExecutableInPath(const std::string& executableName)
	{
		std::filesystem::path direct(executableName);
		std::error_code error;
		if ((direct.is_absolute() || direct.has_parent_path()) && std::filesystem::exists(direct, error))
			return direct;

		std::vector<std::string> candidateNames = { executableName };
#ifdef _WIN32
		if (std::filesystem::path(executableName).extension().empty())
			candidateNames.push_back(executableName + ".exe");
		constexpr char separator = ';';
#else
		constexpr char separator = ':';
#endif

		const char* pathEnv = std::getenv("PATH"); // NOLINT(concurrency-mt-unsafe)
		if (!pathEnv)
			return {};

		std::stringstream stream(pathEnv);
		std::string directory;
		while (std::getline(stream, directory, separator))
		{
			if (directory.empty())
				continue;

			for (const std::string& candidateName : candidateNames)
			{
				std::filesystem::path candidate = std::filesystem::path(directory) / candidateName;
				error.clear();
				if (std::filesystem::exists(candidate, error) && std::filesystem::is_regular_file(candidate, error))
					return candidate;
			}
		}

		return {};
	}

	std::string RunCommandFirstLine(const std::string& command)
	{
#ifdef _WIN32
		std::string firstLine;
		RunWindowsProcess(command, false, &firstLine);
		return firstLine;
#else
		FILE* pipe = popen(command.c_str(), "r");
		if (!pipe)
			return {};

		std::array<char, 1024> buffer{};
		std::string line;
		if (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe))
			line = buffer.data();

		pclose(pipe);

		while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
			line.pop_back();
		return line;
#endif
	}

	std::filesystem::path FindVswhereExecutable()
	{
		if (std::filesystem::path pathCandidate = FindExecutableInPath("vswhere.exe"); !pathCandidate.empty())
			return pathCandidate;

		std::vector<std::filesystem::path> candidates;
		if (std::filesystem::path programFilesX86 = PathFromEnvironment("ProgramFiles(x86)"); !programFilesX86.empty())
			candidates.push_back(programFilesX86 / "Microsoft Visual Studio" / "Installer" / "vswhere.exe");
		if (std::filesystem::path programFiles = PathFromEnvironment("ProgramFiles"); !programFiles.empty())
			candidates.push_back(programFiles / "Microsoft Visual Studio" / "Installer" / "vswhere.exe");

		std::error_code error;
		for (const auto& candidate : candidates)
		{
			if (std::filesystem::exists(candidate, error) && std::filesystem::is_regular_file(candidate, error))
				return candidate;
		}

		return {};
	}

	std::filesystem::path FindMsbuildWithVswhere()
	{
		const std::filesystem::path vswhere = FindVswhereExecutable();
		if (vswhere.empty())
			return {};

		const std::string command = QuoteCommandPath(vswhere) +
			R"( -latest -products * -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe")";
		const std::string firstLine = RunCommandFirstLine(command);
		if (firstLine.empty())
			return {};

		std::error_code error;
		std::filesystem::path candidate(firstLine);
		if (std::filesystem::exists(candidate, error) && std::filesystem::is_regular_file(candidate, error))
			return candidate;
		return {};
	}

	std::filesystem::path FindMsbuildExecutable()
	{
		if (std::filesystem::path envMsbuild = PathFromEnvironment("WHIP_MSBUILD_PATH"); !envMsbuild.empty())
		{
			std::error_code error;
			if (std::filesystem::exists(envMsbuild, error))
				return envMsbuild;
			WHP_EDITOR_WARN(std::string("[Script Build] WHIP_MSBUILD_PATH is set but does not exist: ") + envMsbuild.string());
		}

		if (std::filesystem::path pathMsbuild = FindExecutableInPath("MSBuild.exe"); !pathMsbuild.empty())
			return pathMsbuild;

		if (std::filesystem::path vswhereMsbuild = FindMsbuildWithVswhere(); !vswhereMsbuild.empty())
			return vswhereMsbuild;

		const std::array<std::filesystem::path, 12> candidates =
		{
			"C:/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe",
			"C:/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/amd64/MSBuild.exe",
			"C:/Program Files/Microsoft Visual Studio/2022/Professional/MSBuild/Current/Bin/MSBuild.exe",
			"C:/Program Files/Microsoft Visual Studio/2022/Professional/MSBuild/Current/Bin/amd64/MSBuild.exe",
			"C:/Program Files/Microsoft Visual Studio/2022/Enterprise/MSBuild/Current/Bin/MSBuild.exe",
			"C:/Program Files/Microsoft Visual Studio/2022/Enterprise/MSBuild/Current/Bin/amd64/MSBuild.exe",
			"C:/Program Files/Microsoft Visual Studio/2022/BuildTools/MSBuild/Current/Bin/MSBuild.exe",
			"C:/Program Files/Microsoft Visual Studio/2022/BuildTools/MSBuild/Current/Bin/amd64/MSBuild.exe",
			"C:/Program Files/Microsoft Visual Studio/2022/Preview/MSBuild/Current/Bin/MSBuild.exe",
			"C:/Program Files/Microsoft Visual Studio/2022/Preview/MSBuild/Current/Bin/amd64/MSBuild.exe",
			"C:/Program Files (x86)/Microsoft Visual Studio/2019/Community/MSBuild/Current/Bin/MSBuild.exe",
			"C:/Program Files (x86)/Microsoft Visual Studio/2019/BuildTools/MSBuild/Current/Bin/MSBuild.exe"
		};

		std::error_code error;
		for (const auto& candidate : candidates)
		{
			if (std::filesystem::exists(candidate, error))
				return candidate;
		}

		return {};
	}

	struct ScriptBuildCommand
	{
		std::string command;
		std::string toolName;
	};

	ScriptBuildCommand MakeScriptBuildCommand(const std::filesystem::path& buildFile)
	{
		if (std::filesystem::path msbuild = FindMsbuildExecutable(); !msbuild.empty())
		{
			return {
				.command = QuoteCommandPath(msbuild) + " " + QuoteCommandPath(buildFile) + " /restore /nologo /v:minimal /p:Configuration=Debug /p:Platform=x64 /nr:false",
				.toolName = msbuild.string()
			};
		}

		if (std::filesystem::path dotnet = FindExecutableInPath("dotnet"); !dotnet.empty())
		{
			return {
				.command = QuoteCommandPath(dotnet) + " build " + QuoteCommandPath(buildFile) + " --nologo --restore -v:minimal -p:Configuration=Debug -p:Platform=x64",
				.toolName = dotnet.string()
			};
		}

		return {};
	}

	int RunCommandAndLogOutput(const std::string& command)
	{
#ifdef _WIN32
		return RunWindowsProcess(command, true);
#else
		FILE* pipe = popen(command.c_str(), "r");
		if (!pipe)
		{
			WHP_EDITOR_ERROR("[Script Build] Could not start script build process.");
			return -1;
		}

		std::array<char, 512> buffer{};
		while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe))
		{
			std::string line(buffer.data());
			while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
				line.pop_back();
			if (!line.empty())
				WHP_EDITOR_INFO(std::string("[Script Build] ") + line);
		}

		return pclose(pipe);
#endif
	}
}

EditorLayer::EditorLayer()
	: Layer("Fbox2D"), m_EditorCamera()
{
	m_GizmoType = ImGuizmo::OPERATION::TRANSLATE;
}

void EditorLayer::OnAttach()
{
    WHP_PROFILE_FUNCTION();
	WHP_EDITOR_INFO("[Editor] Attaching EditorLayer.");

	m_AnimationEditorPanel.SetRefreshAssetTreeCallback([this]() {if (m_ContentBrowserPanel) { m_ContentBrowserPanel->RefreshAssetTree(); } });
	m_AssetEditorPanel.SetOpenSceneCallback([this](AssetHandle handle) { OpenScene(handle); });
	m_AssetEditorPanel.SetSetStartSceneCallback([this](AssetHandle handle) { SetStartScene(handle); });
	m_AssetEditorPanel.SetOpenAnimationCallback([this](AssetHandle handle) { return m_AnimationEditorPanel.OpenAsset(handle); });
	m_SceneHierarchyPanel.SetSceneChangeCallback([this]() { CaptureSceneHistory(); });
	m_SceneHierarchyPanel.SetSaveEntityTemplateCallback([this](Entity entityIn) { SaveEntityTemplate(entityIn); });
	m_SceneHierarchyPanel.SetApplyEntityTemplateCallback([this](Entity entityIn) { ApplyEntityTemplate(entityIn); });
	m_SceneHierarchyPanel.SetRevertEntityTemplateCallback([this](Entity entityIn) { RevertEntityTemplate(entityIn); });
	m_SceneHierarchyPanel.SetUnpackEntityTemplateCallback([this](Entity entityIn) { UnpackEntityTemplate(entityIn); });
	m_UIProject.SetSceneCallbacks(
		[this](AssetHandle handle) { OpenScene(handle); },
		[this]() { CloseScene(); },
		[this]() { return m_EditorScenePath; });
	m_UIProject.SetBeforeChangeCallback([this]() { CaptureSceneHistory(true); });
	m_UIProject.SetEditorSettingsDrawer([this]() { m_UISettings.DrawContent(); });
	SetupProjectLoader();
	LoadEditorPreferences();
	m_ProjectLoader.SetRecentProjects(m_RecentProjects);

	// framebuffer
    FramebufferSpecification fbSpec{};
    fbSpec.m_Attachments = { FramebufferTextureFormat::Rgba8, FramebufferTextureFormat::RedInteger, FramebufferTextureFormat::Depth };
    fbSpec.m_Width = Application::Get().GetWindow().GetWidth();
    fbSpec.m_Height = Application::Get().GetWindow().GetHeight();
    m_Framebuffer = Framebuffer::Create(fbSpec);

	// scene
    m_EditorScene = MakeRef<Scene>();
	m_ActiveScene = m_EditorScene;

	// Project
	auto commandLineArgs = Application::Get().GetSpecification().m_CommandLineArgs;
	if (commandLineArgs.m_Count > 1)
	{
		auto projectFilePath = commandLineArgs[1];
		WHP_EDITOR_INFO(std::string("[Project] Opening project from command line: ") + projectFilePath);
		if (OpenProject(projectFilePath))
			m_ProjectLoader.SetLoaded(true);
	}
	// camera
    m_EditorCamera = EditorCamera(30.0f, 1.778f, 0.1f, 1000.0f);
	ConsolePanel::Initialize();
	static float v1 = 0, v2 = 0;
	m_PopupHandler
		.SetPopupName("Popup Testing")
		.SetHeight(300.f)
		.SetWidth(400.f)
		.Add([]() { ImGui::Text("This is a text message for popup testing. Do not mind this Window if you see that."); })
		.Add([]() { static float fv = 0; ImGui::SliderFloat("##Float value", &fv, 0.0f, 10000.0f); })
		.SameLine()
		.Add([]() { static int iv = 0; ImGui::SliderInt("##Int value", &iv, 0, 1000000); })
		.AddDualHandleSlider(0, 100, &v1, &v2)
		.AddButton([this]() { m_PopupHandler.SetShowState(false); }, "Close", 100);

}

void EditorLayer::OnDetach()
{
	WHP_PROFILE_FUNCTION();
	WriteSceneRecoverySnapshot("Editor shutdown");
	StopScriptSourceWatcher();
	SaveEditorPreferences();
	ConsolePanel::Shutdown();

	if (m_SceneState == SceneState::Play)
		m_ActiveScene->OnRuntimeStop();
	else if (m_SceneState == SceneState::Simulate)
		m_ActiveScene->OnSimulationStop();

}

void EditorLayer::OnUpdate(Timestep ts)
{
	WHP_PROFILE_FUNCTION();
	m_Ts = ts;
	ProcessScriptSourceChanges();
	if (m_SceneDirty && m_SceneState == SceneState::Edit)
	{
		const auto now = std::chrono::steady_clock::now();
		if (m_LastSceneRecoverySnapshot == std::chrono::steady_clock::time_point{} || now - m_LastSceneRecoverySnapshot > std::chrono::seconds(30))
			WriteSceneRecoverySnapshot("Autosave");
	}

	{
		WHP_PROFILE_SCOPE("Viewport Size");
		m_ActiveScene->OnViewportResize(static_cast<uint32_t>(m_ViewportSize.x), static_cast<uint32_t>(m_ViewportSize.y));
		if (FramebufferSpecification spec = m_Framebuffer->GetSpecification();
			m_ViewportSize.x > 0.0f &&
			m_ViewportSize.y > 0.0f &&
			(spec.m_Width != static_cast<uint32_t>(m_ViewportSize.x) || spec.m_Height != static_cast<uint32_t>(m_ViewportSize.y)))
		{
			m_Framebuffer->Resize(static_cast<uint32_t>(m_ViewportSize.x), static_cast<uint32_t>(m_ViewportSize.y));
			m_EditorCamera.SetViewportSize(m_ViewportSize.x, m_ViewportSize.y);
		}
	}

	{
		WHP_PROFILE_SCOPE("scene::OnUpdate");
		Renderer2D::ResetStats();
		m_Framebuffer->Bind();
		RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1.0f });
		RenderCommand::Clear();

		m_Framebuffer->ClearAttachment(1, -1);

		switch (m_SceneState)
		{
		case SceneState::Edit:
		{
			if (!m_GizmoUsing)
				m_EditorCamera.OnUpdate(ts);
			DrawEditorGrid();
			m_ActiveScene->OnUpdateEditor(ts, m_EditorCamera);
			break;
		}
		case SceneState::Play:
		{
			m_ActiveScene->OnUpdateRuntime(ts);
			ProcessRuntimeSceneTransition();
			break;
		}
		case SceneState::Simulate:
		{
			if (!m_GizmoUsing)
				m_EditorCamera.OnUpdate(ts);
			DrawEditorGrid();
			m_ActiveScene->OnUpdateSimulation(ts, m_EditorCamera);
			ProcessRuntimeSceneTransition();
			break;
		}
		}
	}

	{
		WHP_PROFILE_SCOPE("Mouse position track");
		auto [mx, my] = ImGui::GetMousePos();
		mx -= m_ViewportBounds[0].x;
		my -= m_ViewportBounds[0].y;
		glm::vec2 viewportSize = m_ViewportBounds[1] - m_ViewportBounds[0];
		my = viewportSize.y - my;
		int mouseX = static_cast<int>(mx);
		int mouseY = static_cast<int>(my);

		if (mouseX >= 0 && mouseY >= 0 && mouseX < static_cast<int>(viewportSize.x) && mouseY < static_cast<int>(viewportSize.y))
		{
			int pixelData = m_Framebuffer->ReadPixel(1, mouseX, mouseY); // This is taking too much time
			m_HoveredEntity = pixelData == -1 ? Entity() : Entity(static_cast<entt::entity>(pixelData), m_ActiveScene.get());
		}
	}

	OnOverlayRender();

    m_Framebuffer->Unbind();
}

_WHP_PRAGMA_WARNING(push)
_WHP_PRAGMA_WARNING_DISABLE(4312)
void EditorLayer::OnImGuiRender()
{
	WHP_PROFILE_FUNCTION();
	ImGuizmo::BeginFrame();
	m_GizmoHovered = false;
	m_GizmoUsing = false;
	const bool projectLoaded = HasProjectLoaded();
	if (!projectLoaded)
	{
		Application::Get().GetImGuiLayer()->BlockEvents(true);

		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->WorkPos);
		ImGui::SetNextWindowSize(viewport->WorkSize);
		ImGui::SetNextWindowViewport(viewport->ID);

		ImGuiWindowFlags hubHostFlags =
			ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImGui::GetStyleColorVec4(ImGuiCol_WindowBg));
		ImGui::Begin("Whip Hub Host", nullptr, hubHostFlags);
		m_ProjectLoader.Run();
		ImGui::End();
		ImGui::PopStyleColor();
		ImGui::PopStyleVar(3);
		return;
	}

	// dockspace
	{
		static bool pOpen = true;
		static ImGuiDockNodeFlags dockspaceFlags = ImGuiDockNodeFlags_None;

		ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;

		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->WorkPos);
		ImGui::SetNextWindowSize(viewport->WorkSize);
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		windowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
		windowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

		if (dockspaceFlags & ImGuiDockNodeFlags_PassthruCentralNode)
			windowFlags |= ImGuiWindowFlags_NoBackground;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("Editor DockSpace", &pOpen, windowFlags);
		ImGui::PopStyleVar(3);

		ImGuiIO& io = ImGui::GetIO();
		ImGuiStyle& style = ImGui::GetStyle();
		float minWinSizeX = style.WindowMinSize.x;
		style.WindowMinSize.x = 300.0f;
		if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
		{
			ImGuiID dockspaceId = ImGui::GetID("Editor DockSpace");
			ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), dockspaceFlags);
		}
		style.WindowMinSize.x = minWinSizeX;
	}
	// menu bar
    if (ImGui::BeginMenuBar())
    {
		auto drawMenuAction = [this](UI::EditorShortcutAction action, const char* label = nullptr)
			{
				std::string shortcut = m_UISettings.GetShortcutLabel(action);
				const bool available = IsEditorActionAvailable(action);
				ImGui::BeginDisabled(!available);
				bool clicked = ImGui::MenuItem(label ? label : UI::UISettings::GetActionDisplayName(action), shortcut.c_str());
				ImGui::EndDisabled();
				if (clicked)
					ExecuteEditorAction(action);
			};

        if (ImGui::BeginMenu("File"))
        {
			drawMenuAction(UI::EditorShortcutAction::OpenProject);
			drawMenuAction(UI::EditorShortcutAction::SaveProject);
			ImGui::Separator();
			drawMenuAction(UI::EditorShortcutAction::NewScene);
			drawMenuAction(UI::EditorShortcutAction::SaveScene);
			drawMenuAction(UI::EditorShortcutAction::SaveSceneAs, "Save Scene As...");
			drawMenuAction(UI::EditorShortcutAction::CloseScene);
			ImGui::Separator();
            if (ImGui::MenuItem("Restart"))
                Application::Get().SubmitToNextTick([]() { Application::Get().Restart(); });
			if (ImGui::MenuItem("Exit"))
				Application::Get().Close();
            ImGui::EndMenu();
        }
		if (ImGui::BeginMenu("Edit"))
		{
			drawMenuAction(UI::EditorShortcutAction::OpenCommandPalette);
			drawMenuAction(UI::EditorShortcutAction::OpenSettings, "Settings");
			ImGui::Separator();
			drawMenuAction(UI::EditorShortcutAction::Undo);
			drawMenuAction(UI::EditorShortcutAction::Redo);
			ImGui::Separator();
			drawMenuAction(UI::EditorShortcutAction::SelectAll);
			drawMenuAction(UI::EditorShortcutAction::Copy);
			drawMenuAction(UI::EditorShortcutAction::Paste);
			drawMenuAction(UI::EditorShortcutAction::Cut);
			drawMenuAction(UI::EditorShortcutAction::DuplicateEntity);
			drawMenuAction(UI::EditorShortcutAction::DeleteEntity);
			ImGui::Separator();
			ImGui::BeginDisabled(!projectLoaded);
			if (ImGui::MenuItem("Show Animation Editor"))
				m_AnimationEditorPanel.Open();
			ImGui::EndDisabled();
			if (ImGui::MenuItem("Show Test Popup"))
				m_PopupHandler.SetShowState(true);

			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Script"))
		{
			drawMenuAction(UI::EditorShortcutAction::ReloadScripts, "Reload Assembly");
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Project"))
		{
			drawMenuAction(UI::EditorShortcutAction::OpenSettings, "Settings");
			ImGui::Separator();
			drawMenuAction(UI::EditorShortcutAction::OpenProject);
			drawMenuAction(UI::EditorShortcutAction::SaveProject);
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Window"))
		{
			auto drawPanelToggle = [](const char* label, bool open, auto&& setter)
				{
					bool requestedOpen = open;
					if (ImGui::MenuItem(label, nullptr, &requestedOpen))
						setter(requestedOpen);
				};

			ImGui::BeginDisabled(!projectLoaded);
			drawPanelToggle("Scene Hierarchy", m_SceneHierarchyPanel.IsOpen(), [this](bool open) { m_SceneHierarchyPanel.SetOpen(open); });
			drawPanelToggle("Statistics", m_UIStatistics.IsOpen(), [this](bool open) { m_UIStatistics.SetOpen(open); });
			drawPanelToggle("Animation Editor", m_AnimationEditorPanel.IsOpen(), [this](bool open) { m_AnimationEditorPanel.SetOpen(open); });
			if (m_ContentBrowserPanel)
				drawPanelToggle("Content Browser", m_ContentBrowserPanel->IsOpen(), [this](bool open) { m_ContentBrowserPanel->SetOpen(open); });
			else
				ImGui::MenuItem("Content Browser", nullptr, false, false);
			ImGui::BeginDisabled(!m_AssetEditorPanel.HasOpenEditors());
			if (ImGui::MenuItem("Close Asset Editors"))
				m_AssetEditorPanel.CloseAll();
			ImGui::EndDisabled();
			ImGui::EndDisabled();
			drawPanelToggle("Console", ConsolePanel::IsOpen(), [](bool open) { ConsolePanel::SetOpen(open); });
			ImGui::EndMenu();
		}

        ImGui::EndMenuBar();
    }
	if (!projectLoaded)
	{
		m_ProjectLoader.Run();
		ImGui::End(); // dockspace
		ConsolePanel::OnImGuiRender();
		m_PopupHandler.OnImGuiRender();
		if (ConsolePanel::ConsumeOpenDirty())
			SaveEditorPreferences();
		return;
	}
	// viewport
	{
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0.0f, 0.0f });
		ImGui::Begin("Viewport");
		auto viewportMinRegion = ImGui::GetWindowContentRegionMin();
		auto viewportMaxRegion = ImGui::GetWindowContentRegionMax();
		auto viewportOffset = ImGui::GetWindowPos();
		m_ViewportBounds[0] = { viewportMinRegion.x + viewportOffset.x, viewportMinRegion.y + viewportOffset.y };
		m_ViewportBounds[1] = { viewportMaxRegion.x + viewportOffset.x, viewportMaxRegion.y + viewportOffset.y };
		m_ViewportFocused = ImGui::IsWindowFocused();
		m_ViewportHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
		Application::Get().GetImGuiLayer()->BlockEvents(!m_ViewportHovered || m_GizmoHovered || m_GizmoUsing);
		ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
		m_ViewportSize = { viewportPanelSize.x, viewportPanelSize.y };

		UI::Image(UI::ToImGuiTextureId(m_Framebuffer->GetColorAttachmentRendererId()), viewportPanelSize, ImVec2{ 0.0f, 1.0f }, ImVec2{ 1.0f, 0.0f });
		if (ImGui::BeginDragDropTarget())
		{
			bool handledDrop = false;
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
			{
				AssetHandle handle = *(AssetHandle*)payload->Data;
				handledDrop = HandleViewportAssetDrop(handle);
			}

			if (!handledDrop)
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_PATH"))
				{
					std::filesystem::path RelativePath(std::string(static_cast<const char*>(payload->Data), payload->DataSize));
					std::filesystem::path absolutePath = Project::GetActiveAssetDirectory() / RelativePath;
					if (std::filesystem::is_regular_file(absolutePath))
					{
						AssetHandle handle = Project::GetActive()->GetEditorAssetManager()->GetHandleFromFilepath(RelativePath);
						if (handle == 0 && Utils::TryGetAssetTypeFromFileExtension(RelativePath.extension()) != AssetType::None)
							handle = Project::GetActive()->GetEditorAssetManager()->ImportAsset(RelativePath);
						HandleViewportAssetDrop(handle);
					}
				}
			}
			ImGui::EndDragDropTarget();
		}

		// gizmos
		Entity selectedEntity = m_SceneHierarchyPanel.GetSelectedEntity();
		if (selectedEntity && m_GizmoType != -1 && m_SceneState != SceneState::Play)
		{
		    ImGuizmo::SetDrawlist();
			ImGuizmo::SetOrthographic(false);
			ImGuizmo::AllowAxisFlip(false);
		    ImGuizmo::SetRect(m_ViewportBounds[0].x, m_ViewportBounds[0].y, m_ViewportBounds[1].x - m_ViewportBounds[0].x, m_ViewportBounds[1].y - m_ViewportBounds[0].y);
		    // Camera
		    const glm::mat4& cameraProjection = m_EditorCamera.GetProjection();
		    glm::mat4 cameraView = m_EditorCamera.GetViewMatrix();

		    // Entity transform
		    auto& tc = selectedEntity.GetComponent<TransformComponent>();
		    glm::mat4 transform = tc.GetTransform();
			const glm::vec3 baseTranslation = tc.m_Translation;
			const glm::vec3 baseRotation = tc.m_Rotation;
			const glm::vec3 baseScale = tc.m_Scale;

		    // Snapping
			const int snapIndex = GizmoSnapIndex(m_GizmoType);
		    bool snap = IsControlDown() && snapIndex != -1;

			ImGuizmo::OPERATION operation = static_cast<ImGuizmo::OPERATION>(m_GizmoType);
			ImGuizmo::Manipulate(
				glm::value_ptr(cameraView),
				glm::value_ptr(cameraProjection),
				operation,
				ImGuizmo::LOCAL,
				glm::value_ptr(transform),
				nullptr,
				snap ? const_cast<float*>(glm::value_ptr(m_UISettings.GetSnapValues(static_cast<uint32_t>(snapIndex)))) : nullptr);
			m_GizmoHovered = ImGuizmo::IsOver(operation);
			m_GizmoUsing = ImGuizmo::IsUsing();

		    if (m_GizmoUsing)
		    {
				if (!m_GizmoHistoryActive)
				{
					CaptureSceneHistory();
					m_GizmoHistoryActive = true;
				}

		        glm::vec3 translation, rotation, scale;
				if (!Math::DecomposeTransform(transform, translation, rotation, scale))
					WHP_CLIENT_WARN("Transform Decomposing error!");

		        glm::vec3 deltaTranslation = translation - baseTranslation;
		        glm::vec3 deltaRotation = rotation - baseRotation;
				glm::vec3 scaleRatio = glm::vec3(1.0f);
				scaleRatio.x = baseScale.x != 0.0f ? scale.x / baseScale.x : 1.0f;
				scaleRatio.y = baseScale.y != 0.0f ? scale.y / baseScale.y : 1.0f;
				scaleRatio.z = baseScale.z != 0.0f ? scale.z / baseScale.z : 1.0f;

				std::vector<Entity> selectedEntities = m_SceneHierarchyPanel.GetSelectedEntities();
				if (std::find(selectedEntities.begin(), selectedEntities.end(), selectedEntity) == selectedEntities.end())
					selectedEntities.push_back(selectedEntity);

				for (Entity selected : selectedEntities)
				{
					if (!selected || !selected.HasComponent<TransformComponent>())
						continue;
					if (selected == selectedEntity)
						continue;

					auto& selectedTransform = selected.GetComponent<TransformComponent>();
					selectedTransform.m_Translation += deltaTranslation;
					selectedTransform.m_Rotation += deltaRotation;
					selectedTransform.m_Scale *= scaleRatio;
				}

		        tc.m_Translation = translation;
		        tc.m_Rotation = rotation;
		        tc.m_Scale = scale;
		    }
		}
		if (!m_GizmoUsing)
			m_GizmoHistoryActive = false;
		UIToolbar();
		Application::Get().GetImGuiLayer()->BlockEvents(!m_ViewportHovered || m_GizmoHovered || m_GizmoUsing);
		ImGui::End();
		ImGui::PopStyleVar();
	} // viewport

	m_UIProject.OnImGuiRender(); // should be in dockspace

    ImGui::End(); // dockspace

	// other renders
	m_UIStatistics.OnImGuiRender(m_Ts);
    m_SceneHierarchyPanel.OnImGuiRender();
	m_AssetEditorPanel.OnImGuiRender();
    m_AnimationEditorPanel.OnImGuiRender();
	m_AnimationEditorPanel.HandleShortcutInput(m_UISettings);
	ConsolePanel::OnImGuiRender();
	if (m_ContentBrowserPanel)
		m_ContentBrowserPanel->OnImGuiRender();
	DrawCommandPalette();
	if (m_UISettings.ConsumeDirty()
		|| m_SceneHierarchyPanel.ConsumeOpenDirty()
		|| m_AssetEditorPanel.ConsumeOpenDirty()
		|| m_AnimationEditorPanel.ConsumeOpenDirty()
		|| m_UIStatistics.ConsumeOpenDirty()
		|| ConsolePanel::ConsumeOpenDirty()
		|| (m_ContentBrowserPanel && m_ContentBrowserPanel->ConsumePreferencesDirty()))
		SaveEditorPreferences();
	m_PopupHandler.OnImGuiRender();

}
_WHP_PRAGMA_WARNING(pop)

void EditorLayer::OnEvent(Event& event)
{
	if (m_SceneState == SceneState::Edit && !m_GizmoHovered && !m_GizmoUsing && Application::Get().GetImGuiLayer()->GetActiveWidgetID() == 0)
		m_EditorCamera.OnEvent(event);
    EventDispatcher dispatcher(event);
    dispatcher.Dispatch<KeyPressedEvent>([this](auto&&... args) -> decltype(auto) { return this->OnKeyPressed(std::forward<decltype(args)>(args)...); });
    dispatcher.Dispatch<MouseButtonPressedEvent>([this](auto&&... args) -> decltype(auto) { return this->OnMouseButtonPressed(std::forward<decltype(args)>(args)...); });
	dispatcher.Dispatch<WindowDropEvent>([this](auto&&... args) -> decltype(auto) { return this->OnWindowDrop(std::forward<decltype(args)>(args)...); });
}

bool EditorLayer::OnKeyPressed(KeyPressedEvent& event)
{
    // Shortcuts
    if (event.GetRepeatCount() > 0)
        return false;

    bool control = Input::IsKeyDown(Key::LeftControl) || Input::IsKeyDown(Key::RightControl);
    bool shift = Input::IsKeyDown(Key::LeftShift) || Input::IsKeyDown(Key::RightShift);
    bool alt = Input::IsKeyDown(Key::LeftAlt) || Input::IsKeyDown(Key::RightAlt);
	const bool hasActiveWidget = Application::Get().GetImGuiLayer()->GetActiveWidgetID() != 0;

	for (size_t i = 0; i < UI::UISettings::ActionCount; ++i)
	{
		UI::EditorShortcutAction action = static_cast<UI::EditorShortcutAction>(i);
		if (m_UISettings.ShortcutMatches(action, event.GetKeyCode(), control, shift, alt))
		{
			if (hasActiveWidget &&
				action != UI::EditorShortcutAction::OpenCommandPalette &&
				action != UI::EditorShortcutAction::Play &&
				action != UI::EditorShortcutAction::Simulate &&
				action != UI::EditorShortcutAction::Stop &&
				action != UI::EditorShortcutAction::Pause)
			{
				return false;
			}

			if (m_AnimationEditorPanel.WantsShortcutCapture() && m_AnimationEditorPanel.ShouldConsumeShortcutAction(action))
				return true;
			return ExecuteEditorAction(action);
		}
	}

    return false;
}

bool EditorLayer::ExecuteEditorAction(UI::EditorShortcutAction action)
{
	if (!IsEditorActionAvailable(action))
		return false;

	switch (action)
	{
	case UI::EditorShortcutAction::OpenCommandPalette:
		OpenCommandPalette();
		return true;
	case UI::EditorShortcutAction::OpenSettings:
		m_UIProject.Show(UI::UIProject::UISettings, [this]() -> decltype(auto) { return this->FinishProjectSettings(); });
		return true;
	case UI::EditorShortcutAction::OpenProject:
		OpenProject();
		return true;
	case UI::EditorShortcutAction::NewScene:
		NewScene();
		return true;
	case UI::EditorShortcutAction::SaveScene:
		SaveScene();
		return true;
	case UI::EditorShortcutAction::SaveSceneAs:
		SaveSceneAs();
		return true;
	case UI::EditorShortcutAction::SaveProject:
		SaveProject();
		return true;
	case UI::EditorShortcutAction::CloseScene:
		CloseScene();
		return true;
	case UI::EditorShortcutAction::ReloadScripts:
		ReloadAssembly(true);
		return true;
	case UI::EditorShortcutAction::DuplicateEntity:
		OnDuplicatedEntity();
		return true;
	case UI::EditorShortcutAction::DeleteEntity:
		OnDeletedEntity();
		return true;
	case UI::EditorShortcutAction::Undo:
		UndoScene();
		return true;
	case UI::EditorShortcutAction::Redo:
		RedoScene();
		return true;
	case UI::EditorShortcutAction::SelectAll:
		OnSelectAllEntities();
		return true;
	case UI::EditorShortcutAction::Copy:
		OnCopyEntities();
		return true;
	case UI::EditorShortcutAction::Paste:
		OnPasteEntities();
		return true;
	case UI::EditorShortcutAction::Cut:
		OnCutEntities();
		return true;
	case UI::EditorShortcutAction::Play:
		if (m_SceneState == SceneState::Edit)
			OnScenePlay();
		else if (m_SceneState == SceneState::Play)
			OnSceneStop();
		return true;
	case UI::EditorShortcutAction::Simulate:
		if (m_SceneState == SceneState::Edit)
			OnSceneSimulate();
		else if (m_SceneState == SceneState::Simulate)
			OnSceneStop();
		return true;
	case UI::EditorShortcutAction::Stop:
		OnSceneStop();
		return true;
	case UI::EditorShortcutAction::Pause:
		m_ActiveScene->SetPaused(!m_ActiveScene->IsPaused());
		return true;
	case UI::EditorShortcutAction::GizmoNone:
		m_GizmoType = -1;
		return true;
	case UI::EditorShortcutAction::GizmoTranslate:
		m_GizmoType = ImGuizmo::OPERATION::TRANSLATE;
		return true;
	case UI::EditorShortcutAction::GizmoRotate:
		m_GizmoType = ImGuizmo::OPERATION::ROTATE;
		return true;
	case UI::EditorShortcutAction::GizmoScale:
		m_GizmoType = ImGuizmo::OPERATION::SCALE;
		return true;
	default:
		return false;
	}
}

bool EditorLayer::IsEditorActionAvailable(UI::EditorShortcutAction action) const
{
	const bool projectLoaded = HasProjectLoaded();
	const bool editMode = m_SceneState == SceneState::Edit;
	const bool hasSelection = (bool)m_SceneHierarchyPanel.GetSelectedEntity();

	switch (action)
	{
	case UI::EditorShortcutAction::OpenProject:
	case UI::EditorShortcutAction::OpenCommandPalette:
		return true;
	case UI::EditorShortcutAction::OpenSettings:
		return projectLoaded;
	case UI::EditorShortcutAction::NewScene:
	case UI::EditorShortcutAction::SaveScene:
	case UI::EditorShortcutAction::SaveSceneAs:
	case UI::EditorShortcutAction::SaveProject:
	case UI::EditorShortcutAction::CloseScene:
	case UI::EditorShortcutAction::ReloadScripts:
	case UI::EditorShortcutAction::SelectAll:
		return projectLoaded && editMode;
	case UI::EditorShortcutAction::DuplicateEntity:
	case UI::EditorShortcutAction::DeleteEntity:
	case UI::EditorShortcutAction::Copy:
	case UI::EditorShortcutAction::Cut:
		return projectLoaded && editMode && hasSelection;
	case UI::EditorShortcutAction::Paste:
		return projectLoaded && editMode && !m_EntityClipboard.empty();
	case UI::EditorShortcutAction::Undo:
		return projectLoaded && editMode && !m_UndoStack.empty();
	case UI::EditorShortcutAction::Redo:
		return projectLoaded && editMode && !m_RedoStack.empty();
	case UI::EditorShortcutAction::Play:
		return projectLoaded && m_SceneState != SceneState::Simulate;
	case UI::EditorShortcutAction::Simulate:
		return projectLoaded && m_SceneState != SceneState::Play;
	case UI::EditorShortcutAction::Stop:
		return m_SceneState == SceneState::Play || m_SceneState == SceneState::Simulate;
	case UI::EditorShortcutAction::Pause:
		return projectLoaded && m_SceneState != SceneState::Edit;
	case UI::EditorShortcutAction::GizmoNone:
	case UI::EditorShortcutAction::GizmoTranslate:
	case UI::EditorShortcutAction::GizmoRotate:
	case UI::EditorShortcutAction::GizmoScale:
		return projectLoaded && editMode && !m_GizmoUsing;
	default:
		return false;
	}
}

void EditorLayer::OpenCommandPalette()
{
	m_CommandPaletteOpen = true;
	m_CommandPaletteFocusSearch = true;
	m_CommandPaletteFilter[0] = '\0';
}

void EditorLayer::DrawCommandPalette()
{
	if (!m_CommandPaletteOpen)
		return;

	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f, viewport->WorkPos.y + viewport->WorkSize.y * 0.22f), ImGuiCond_Appearing, ImVec2(0.5f, 0.0f));
	ImGui::SetNextWindowSize(ImVec2(680.0f, 460.0f), ImGuiCond_Appearing);

	bool open = true;
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;
	if (ImGui::Begin("Command Palette", &open, flags))
	{
		if (m_CommandPaletteFocusSearch)
		{
			ImGui::SetKeyboardFocusHere();
			m_CommandPaletteFocusSearch = false;
		}

		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputTextWithHint("##CommandPaletteSearch", "Search commands...", m_CommandPaletteFilter, sizeof(m_CommandPaletteFilter));
		ImGui::Spacing();
		ImGui::Separator();

		UI::EditorShortcutAction firstAvailableAction = UI::EditorShortcutAction::Count;
		bool hasVisibleCommand = false;

		if (ImGui::BeginChild("##CommandPaletteResults", ImVec2(0.0f, 0.0f), false))
		{
			if (ImGui::BeginTable("##CommandPaletteTable", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
			{
				ImGui::TableSetupColumn("Command", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("Group", ImGuiTableColumnFlags_WidthFixed, 110.0f);
				ImGui::TableSetupColumn("Shortcut", ImGuiTableColumnFlags_WidthFixed, 150.0f);

				for (UI::EditorShortcutAction action : CommandPaletteActions)
				{
					if (!CommandMatchesFilter(action, m_CommandPaletteFilter))
						continue;

					hasVisibleCommand = true;
					const bool available = IsEditorActionAvailable(action);
					if (available && firstAvailableAction == UI::EditorShortcutAction::Count)
						firstAvailableAction = action;

					ImGui::PushID(static_cast<int>(action));
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::BeginDisabled(!available);
					if (ImGui::Selectable(UI::UISettings::GetActionDisplayName(action), false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0.0f, 30.0f)))
					{
						if (ExecuteEditorAction(action) && action != UI::EditorShortcutAction::OpenCommandPalette)
							m_CommandPaletteOpen = false;
					}
					ImGui::TableNextColumn();
					ImGui::TextDisabled("%s", UI::UISettings::GetActionCategory(action));
					ImGui::TableNextColumn();
					const std::string shortcut = m_UISettings.GetShortcutLabel(action);
					if (m_UISettings.HasShortcutConflict(action))
						ImGui::TextColored(ImVec4(0.95f, 0.50f, 0.34f, 1.0f), "Conflict");
					else
						ImGui::TextDisabled("%s", shortcut.c_str());
					ImGui::EndDisabled();
					ImGui::PopID();
				}

				ImGui::EndTable();
			}

			if (!hasVisibleCommand)
				ImGui::TextDisabled("No commands found.");
		}
		ImGui::EndChild();

		if (ImGui::IsKeyPressed(ImGuiKey_Escape))
			m_CommandPaletteOpen = false;
		if (firstAvailableAction != UI::EditorShortcutAction::Count && ImGui::IsKeyPressed(ImGuiKey_Enter))
		{
			if (ExecuteEditorAction(firstAvailableAction) && firstAvailableAction != UI::EditorShortcutAction::OpenCommandPalette)
				m_CommandPaletteOpen = false;
		}
	}
	ImGui::End();

	if (!open)
		m_CommandPaletteOpen = false;
}

bool EditorLayer::OnMouseButtonPressed(MouseButtonPressedEvent& event)
{
    if (event.GetMouseButton() == Mouse::ButtonLeft)
    {
        if (m_ViewportHovered && !m_GizmoHovered && !m_GizmoUsing && !Input::IsKeyDown(Key::LeftAlt) && Application::Get().GetImGuiLayer()->GetActiveWidgetID() == 0)
		{
			bool append = IsControlDown();
            m_SceneHierarchyPanel.SetSelectedEntity(m_HoveredEntity, append);
		}
    }
    return false;
}

bool EditorLayer::OnWindowDrop(WindowDropEvent& event)
{
	if (!HasProjectLoaded())
		return false;

	if (m_ContentBrowserPanel && m_ContentBrowserPanel->IsHovered())
		return m_ContentBrowserPanel->HandleExternalDrop(event.GetPaths());

	bool handled = false;
	for (const auto& path : event.GetPaths())
	{
		AssetHandle handle = ImportExternalAssetFile(path);
		if (handle != 0)
		{
			handled = true;
			if (m_ViewportHovered)
				HandleViewportAssetDrop(handle);
		}
	}
	if (m_ContentBrowserPanel)
		m_ContentBrowserPanel->RefreshAssetTree();
	return handled;
}

bool EditorLayer::HandleViewportAssetDrop(AssetHandle handle)
{
	if (handle == 0 || !HasProjectLoaded())
		return false;

	Ref<Project> activeProject = Project::GetActive();
	if (!activeProject->GetEditorAssetManager()->IsAssetHandleValid(handle))
		return false;

	const AssetType type = activeProject->GetEditorAssetManager()->GetAssetType(handle);
	switch (type)
	{
	case AssetType::Scene:
		OpenScene(handle);
		return true;
	case AssetType::Entity:
		return InstantiateEntityTemplate(handle);
	case AssetType::Texture2D:
		return CreateSpriteEntityFromTexture(handle, GetViewportMouseWorldPosition());
	default:
		WHP_EDITOR_WARN("[Viewport] This Asset type cannot be dropped into the viewport yet.");
		return false;
	}
}

bool EditorLayer::HandleContentBrowserAssetOpen(AssetHandle handle)
{
	if (handle == 0 || !HasProjectLoaded())
		return false;

	Ref<Project> activeProject = Project::GetActive();
	if (!activeProject->GetEditorAssetManager()->IsAssetHandleValid(handle))
		return false;

	switch (activeProject->GetEditorAssetManager()->GetAssetType(handle))
	{
	case AssetType::Scene:
		OpenScene(handle);
		return true;
	case AssetType::Entity:
		return InstantiateEntityTemplate(handle);
	default:
		return false;
	}
}

bool EditorLayer::HandleContentBrowserAssetInspect(AssetHandle handle)
{
	if (handle == 0 || !HasProjectLoaded())
		return false;

	Ref<Project> activeProject = Project::GetActive();
	if (!activeProject->GetEditorAssetManager()->IsAssetHandleValid(handle))
		return false;

	const AssetType type = activeProject->GetEditorAssetManager()->GetAssetType(handle);
	if (type == AssetType::Animation || type == AssetType::AnimationController)
		return m_AnimationEditorPanel.OpenAsset(handle);

	m_AssetEditorPanel.OpenAsset(handle);
	return true;
}

void EditorLayer::SetStartScene(AssetHandle handle)
{
	if (handle == 0 || !HasProjectLoaded())
		return;

	Ref<Project> activeProject = Project::GetActive();
	if (!activeProject->GetEditorAssetManager()->IsAssetHandleValid(handle) ||
		activeProject->GetEditorAssetManager()->GetAssetType(handle) != AssetType::Scene)
	{
		return;
	}

	activeProject->GetConfig().m_StartScene = handle;
	Project::SaveActive();
	WHP_EDITOR_INFO(std::string("[Project] Start scene set: ") + activeProject->GetEditorAssetManager()->GetFilepath(handle).generic_string());
}

bool EditorLayer::CreateSpriteEntityFromTexture(AssetHandle handle, const glm::vec3& position)
{
	if (!HasProjectLoaded() || !m_EditorScene || m_SceneState != SceneState::Edit)
		return false;

	Ref<Project> activeProject = Project::GetActive();
	if (!activeProject->GetEditorAssetManager()->IsAssetHandleValid(handle) ||
		activeProject->GetEditorAssetManager()->GetAssetType(handle) != AssetType::Texture2D)
	{
		return false;
	}

	CaptureSceneHistory();
	const auto& metadata = activeProject->GetEditorAssetManager()->GetMetadata(handle);
	const std::string name = metadata.m_Filepath.stem().empty() ? "Sprite" : metadata.m_Filepath.stem().string();
	Entity sprite = m_EditorScene->CreateEntity(name);
	auto& transform = sprite.GetComponent<TransformComponent>();
	transform.m_Translation = position;

	auto texture = AssetManager::GetAsset<Texture2D>(handle);
	if (texture && texture->IsLoaded())
	{
		constexpr float pixelsPerUnit = 100.0f;
		transform.m_Scale = {
			glm::max(texture->GetWidth() / pixelsPerUnit, 0.1f),
			glm::max(texture->GetHeight() / pixelsPerUnit, 0.1f),
			1.0f
		};
	}

	auto& spriteRenderer = sprite.AddComponent<SpriteRendererComponent>();
	spriteRenderer.m_Texture = handle;
	spriteRenderer.m_Color = glm::vec4(1.0f);
	m_SceneHierarchyPanel.SetSelectedEntity(sprite);
	WHP_EDITOR_INFO(std::string("[Viewport] Created sprite entity from texture ") + metadata.m_Filepath.generic_string());
	return true;
}

AssetHandle EditorLayer::ImportExternalAssetFile(const std::filesystem::path& sourcePath)
{
	if (!HasProjectLoaded())
		return 0;

	std::error_code error;
	if (!std::filesystem::exists(sourcePath, error) || !std::filesystem::is_regular_file(sourcePath, error))
		return 0;

	const AssetType type = Utils::TryGetAssetTypeFromFileExtension(sourcePath.extension());
	if (type == AssetType::None)
	{
		WHP_EDITOR_WARN(std::string("[Asset Import] Unsupported dropped file: ") + sourcePath.string());
		return 0;
	}

	const std::filesystem::path assetDirectory = Project::GetActiveAssetDirectory();
	std::filesystem::path assetPath = sourcePath;
	if (!PathIsOrIsUnder(sourcePath, assetDirectory))
	{
		const std::filesystem::path importDirectory = assetDirectory / DefaultImportDirectoryForType(type);
		std::filesystem::create_directories(importDirectory, error);
		if (error)
		{
			WHP_EDITOR_WARN(std::string("[Asset Import] Could not create import directory: ") + error.message());
			return 0;
		}

		assetPath = MakeUniquePath(importDirectory / sourcePath.filename());
		error.clear();
		std::filesystem::copy_file(sourcePath, assetPath, std::filesystem::copy_options::none, error);
		if (error)
		{
			WHP_EDITOR_WARN(std::string("[Asset Import] Could not copy dropped file: ") + error.message());
			return 0;
		}
	}

	error.clear();
	std::filesystem::path RelativePath = std::filesystem::relative(assetPath, assetDirectory, error).lexically_normal();
	if (error)
		return 0;

	if (AssetHandle existingHandle = Project::GetActive()->GetEditorAssetManager()->GetHandleFromFilepath(RelativePath); existingHandle != 0)
		return existingHandle;

	return Project::GetActive()->GetEditorAssetManager()->ImportAsset(RelativePath);
}

glm::vec3 EditorLayer::GetViewportMouseWorldPosition() const
{
	const glm::vec2 viewportSize = m_ViewportBounds[1] - m_ViewportBounds[0];
	const glm::vec3 fallback = m_EditorCamera.GetPosition() + m_EditorCamera.GetForwardDirection() * glm::max(m_EditorCamera.GetDistance(), 1.0f);
	if (viewportSize.x <= 0.0f || viewportSize.y <= 0.0f)
		return { fallback.x, fallback.y, 0.0f };

	const ImVec2 mouse = ImGui::GetMousePos();
	const float x = glm::clamp((mouse.x - m_ViewportBounds[0].x) / viewportSize.x, 0.0f, 1.0f);
	const float y = glm::clamp((mouse.y - m_ViewportBounds[0].y) / viewportSize.y, 0.0f, 1.0f);
	const glm::vec2 ndc{ x * 2.0f - 1.0f, (1.0f - y) * 2.0f - 1.0f };

	const glm::mat4 inverseViewProjection = glm::inverse(m_EditorCamera.GetViewProjection());
	glm::vec4 nearPoint = inverseViewProjection * glm::vec4(ndc.x, ndc.y, -1.0f, 1.0f);
	glm::vec4 farPoint = inverseViewProjection * glm::vec4(ndc.x, ndc.y, 1.0f, 1.0f);
	nearPoint /= nearPoint.w;
	farPoint /= farPoint.w;

	const glm::vec3 rayOrigin = glm::vec3(nearPoint);
	const glm::vec3 rayDirection = glm::normalize(glm::vec3(farPoint - nearPoint));
	if (glm::abs(rayDirection.z) < 0.0001f)
		return { fallback.x, fallback.y, 0.0f };

	const float t = -rayOrigin.z / rayDirection.z;
	const glm::vec3 worldPosition = rayOrigin + rayDirection * t;
	return { worldPosition.x, worldPosition.y, 0.0f };
}

void EditorLayer::DrawEditorGrid()
{
	if (m_ViewportSize.x <= 0.0f || m_ViewportSize.y <= 0.0f)
		return;

	const float aspectRatio = m_ViewportSize.x / m_ViewportSize.y;
	const float distance = glm::max(m_EditorCamera.GetDistance(), 1.0f);
	const float visibleHeight = distance * 1.15f;
	const float visibleWidth = visibleHeight * aspectRatio;
	const glm::vec3 center = m_EditorCamera.GetPosition() + m_EditorCamera.GetForwardDirection() * distance;

	float gridStep = 1.0f;
	const float visibleSpan = glm::max(visibleWidth, visibleHeight);
	while ((visibleSpan / gridStep) > 240.0f)
		gridStep *= 2.0f;

	const int minX = static_cast<int>(std::floor((center.x - visibleWidth * 0.5f) / gridStep)) - 2;
	const int maxX = static_cast<int>(std::ceil((center.x + visibleWidth * 0.5f) / gridStep)) + 2;
	const int minY = static_cast<int>(std::floor((center.y - visibleHeight * 0.5f) / gridStep)) - 2;
	const int maxY = static_cast<int>(std::ceil((center.y + visibleHeight * 0.5f) / gridStep)) + 2;

	const glm::vec4 gridColor{ 0.26f, 0.29f, 0.30f, 0.34f };
	const glm::vec4 majorGridColor{ 0.37f, 0.41f, 0.41f, 0.45f };
	const glm::vec4 xAxisColor{ 0.86f, 0.34f, 0.30f, 0.74f };
	const glm::vec4 yAxisColor{ 0.30f, 0.66f, 0.46f, 0.74f };
	const float minZ = -0.02f;
	auto isMajorGridLine = [](float value)
	{
		return std::fmod(std::abs(value), 10.0f) < 0.0001f;
	};

	Renderer2D::BeginScene(m_EditorCamera);
	Renderer2D::SetLineWidth(1.0f);

	for (int x = minX; x <= maxX; ++x)
	{
		const float worldX = static_cast<float>(x) * gridStep;
		const bool isAxis = std::abs(worldX) < 0.0001f;
		const bool isMajor = isMajorGridLine(worldX);
		const glm::vec4& color = isAxis ? yAxisColor : (isMajor ? majorGridColor : gridColor);
		Renderer2D::DrawLine(
			{ worldX, static_cast<float>(minY) * gridStep, minZ },
			{ worldX, static_cast<float>(maxY) * gridStep, minZ },
			color);
	}

	for (int y = minY; y <= maxY; ++y)
	{
		const float worldY = static_cast<float>(y) * gridStep;
		const bool isAxis = std::abs(worldY) < 0.0001f;
		const bool isMajor = isMajorGridLine(worldY);
		const glm::vec4& color = isAxis ? xAxisColor : (isMajor ? majorGridColor : gridColor);
		Renderer2D::DrawLine(
			{ static_cast<float>(minX) * gridStep, worldY, minZ },
			{ static_cast<float>(maxX) * gridStep, worldY, minZ },
			color);
	}

	Renderer2D::EndScene();
}

void EditorLayer::OnOverlayRender()
{
	WHP_PROFILE_FUNCTION();
	if (m_SceneState == SceneState::Play)
	{
		Entity cam = m_ActiveScene->GetPrimaryCameraEntity();
		if (!cam)
			return;
		Renderer2D::BeginScene(cam.GetComponent<CameraComponent>().m_Camera, cam.GetComponent<TransformComponent>().GetTransform());
	}
	else
	{
		Renderer2D::BeginScene(m_EditorCamera);
	}

	if (m_UISettings.GetShowPhysicsColliders())
	{
		// Box Colliders
		{
			auto view = m_ActiveScene->GetAllEntitiesWith<TransformComponent, BoxCollider2DComponent>();
			for (auto Entity : view)
			{
				auto [tc, bc2d] = view.get<TransformComponent, BoxCollider2DComponent>(Entity);

				glm::vec3 translation = tc.m_Translation + glm::vec3(bc2d.m_Offset, 0.001f);
				glm::vec3 scale = tc.m_Scale * glm::vec3(bc2d.m_Size * 2.0f, 1.0f);

				glm::mat4 transform = glm::translate(glm::mat4(1.0f), tc.m_Translation)
					* glm::rotate(glm::mat4(1.0f), tc.m_Rotation.z, glm::vec3(0.0f, 0.0f, 1.0f))
					* glm::translate(glm::mat4(1.0f), glm::vec3(bc2d.m_Offset, 0.001f))
					* glm::scale(glm::mat4(1.0f), scale);

				Renderer2D::DrawRect(transform, glm::vec4(0, 1, 0, 1));
			}
		}

		// Circle Colliders
		{
			auto view = m_ActiveScene->GetAllEntitiesWith<TransformComponent, CircleCollider2DComponent>();
			for (auto Entity : view)
			{
				auto [tc, cc2d] = view.get<TransformComponent, CircleCollider2DComponent>(Entity);

				glm::vec3 translation = tc.m_Translation + glm::vec3(cc2d.m_Offset, 0.001f);
				glm::vec3 scale = tc.m_Scale * glm::vec3(cc2d.m_Radius * 2.0f);

				glm::mat4 transform = glm::translate(glm::mat4(1.0f), translation)
					* glm::scale(glm::mat4(1.0f), scale);

				Renderer2D::DrawCircle(transform, glm::vec4(0, 1, 0, 1), 0.02f);
			}
		}
	}

	for (Entity selectedEntity : m_SceneHierarchyPanel.GetSelectedEntities())
	{
		TransformComponent transform = selectedEntity.GetComponent<TransformComponent>();
		if (selectedEntity.HasComponent<TextComponent>() && !selectedEntity.HasComponent<SpriteRendererComponent>() && !selectedEntity.HasComponent<CircleRendererComponent>())
		{
			selectedEntity.GetComponent<TextComponent>();
		}
		else
			Renderer2D::DrawRect(transform.GetTransform(), glm::vec4(0.9f, 0.4f, 0.1f, 1.0f));
	}

	Renderer2D::EndScene();
}

bool EditorLayer::HasProjectLoaded() const
{
	return Project::GetActive() != nullptr;
}

void EditorLayer::SetupProjectLoader()
{
	m_ProjectLoader.SetCreateProjectCallback([this](const UI::ProjectCreateSettings& settings) { return NewProject(settings); });
	m_ProjectLoader.SetLoadProjectCallback([this]() { return OpenProject(); });
	m_ProjectLoader.SetOpenRecentProjectCallback([this](const std::filesystem::path& path) {
		std::error_code error;
		if (!std::filesystem::exists(path, error))
		{
			WHP_EDITOR_WARN("[Whip Hub] Recent Project no longer exists.");
			m_ProjectLoader.SetStatus("Recent project no longer exists.");
			LoadRecentProjects();
			return false;
		}

		const bool opened = OpenProject(path);
		if (!opened)
			m_ProjectLoader.SetStatus("Project could not be opened.");
		return opened;
	});
	m_ProjectLoader.SetForgetRecentProjectCallback([this](const std::filesystem::path& path) {
		return ForgetRecentProject(path);
	});
	m_ProjectLoader.SetDeleteRecentProjectCallback([this](const std::filesystem::path& path) {
		return DeleteRecentProject(path);
	});
}

void EditorLayer::LoadRecentProjects()
{
	m_RecentProjects.clear();
	bool shouldRewrite = false;

	std::ifstream stream(GetRecentProjectsPath());
	if (!stream)
	{
		m_ProjectLoader.SetRecentProjects(m_RecentProjects);
		return;
	}

	std::string line;
	while (std::getline(stream, line))
	{
		if (line.empty())
			continue;

		std::filesystem::path path(line);
		std::error_code error;
		if (!std::filesystem::exists(path, error) || !FileExtensions::IsProjectExtension(path))
		{
			shouldRewrite = true;
			continue;
		}

		path = std::filesystem::weakly_canonical(path, error);
		if (error)
			path = std::filesystem::absolute(line, error);
		if (!ShouldIncludeRecentProject(path))
		{
			shouldRewrite = true;
			continue;
		}

		if (std::find(m_RecentProjects.begin(), m_RecentProjects.end(), path) == m_RecentProjects.end())
			m_RecentProjects.push_back(path);
		else
			shouldRewrite = true;

		if (m_RecentProjects.size() >= 10)
		{
			shouldRewrite = true;
			break;
		}
	}

	stream.close();
	if (shouldRewrite)
		SaveRecentProjects();
	m_ProjectLoader.SetRecentProjects(m_RecentProjects);
}

void EditorLayer::SaveRecentProjects() const
{
	std::ofstream stream(GetRecentProjectsPath(), std::ios::trunc);
	if (!stream)
		return;

	for (const auto& projectPath : m_RecentProjects)
		stream << projectPath.string() << '\n';
}

void EditorLayer::AddRecentProject(const std::filesystem::path& path)
{
	if (path.empty())
		return;

	std::error_code error;
	std::filesystem::path normalizedPath = std::filesystem::weakly_canonical(path, error);
	if (error)
	{
		error.clear();
		normalizedPath = std::filesystem::absolute(path, error);
	}
	if (error)
		normalizedPath = path;
	if (!ShouldIncludeRecentProject(normalizedPath))
		return;

	m_RecentProjects.erase(
		std::remove(m_RecentProjects.begin(), m_RecentProjects.end(), normalizedPath),
		m_RecentProjects.end());

	m_RecentProjects.insert(m_RecentProjects.begin(), normalizedPath);
	if (m_RecentProjects.size() > 10)
		m_RecentProjects.resize(10);

	m_LastProjectPath = normalizedPath;
	SaveRecentProjects();
	SaveEditorPreferences();
	m_ProjectLoader.SetRecentProjects(m_RecentProjects);
}

bool EditorLayer::ForgetRecentProject(const std::filesystem::path& path)
{
	if (path.empty())
		return false;

	const size_t previousSize = m_RecentProjects.size();
	m_RecentProjects.erase(
		std::remove_if(m_RecentProjects.begin(), m_RecentProjects.end(),
			[&path](const std::filesystem::path& recentPath)
			{
				return PathsMatchForRecentProject(recentPath, path);
			}),
		m_RecentProjects.end());

	if (m_RecentProjects.size() == previousSize)
		return false;

	if (PathsMatchForRecentProject(m_LastProjectPath, path))
		m_LastProjectPath.clear();

	SaveRecentProjects();
	SaveEditorPreferences();
	m_ProjectLoader.SetRecentProjects(m_RecentProjects);
	return true;
}

bool EditorLayer::DeleteRecentProject(const std::filesystem::path& path)
{
	if (path.empty() || !FileExtensions::IsProjectExtension(path))
		return false;

	std::error_code error;
	std::filesystem::path projectPath = NormalizeProjectListPath(path);
	const bool projectFileExists = std::filesystem::exists(projectPath, error);
	if (error)
		return false;
	if (!projectFileExists)
		return ForgetRecentProject(path);

	if (!std::filesystem::is_regular_file(projectPath, error) || error)
		return false;

	const std::filesystem::path projectDirectory = NormalizeProjectListPath(projectPath.parent_path());
	if (projectDirectory.empty() || projectDirectory == projectDirectory.root_path())
		return false;

	if (projectDirectory.filename() != projectPath.stem())
	{
		WHP_EDITOR_WARN(std::string("[Whip Hub] Refusing to delete Project folder because it does not match the Project file name: ") + projectDirectory.string());
		return false;
	}

	error.clear();
	const std::filesystem::path workingDirectory = NormalizeProjectListPath(std::filesystem::current_path());
	if (!workingDirectory.empty() && PathIsOrIsUnder(workingDirectory, projectDirectory))
	{
		WHP_EDITOR_WARN(std::string("[Whip Hub] Refusing to delete a Project folder that contains the editor working directory: ") + projectDirectory.string());
		return false;
	}

	Ref<Project> activeProject = Project::GetActive();
	if (activeProject && PathsMatchForRecentProject(activeProject->GetProjectPath(), projectPath))
	{
		WHP_EDITOR_WARN("[Whip Hub] Refusing to delete the currently loaded Project.");
		return false;
	}

	std::filesystem::remove_all(projectDirectory, error);
	if (error)
	{
		WHP_EDITOR_ERROR(std::string("[Whip Hub] Could not delete Project folder ") + projectDirectory.string() + ": " + error.message());
		return false;
	}

	return ForgetRecentProject(projectPath);
}

bool EditorLayer::ShouldIncludeRecentProject(const std::filesystem::path& path) const
{
	std::error_code error;
	std::filesystem::path normalizedPath = std::filesystem::weakly_canonical(path, error);
	if (error)
		normalizedPath = path;

	error.clear();
	std::filesystem::path workingDirectory = std::filesystem::weakly_canonical(std::filesystem::current_path(), error);
	if (error)
		workingDirectory = std::filesystem::current_path();

	error.clear();
	std::filesystem::path RelativePath = std::filesystem::relative(normalizedPath, workingDirectory, error);
	if (!error && !RelativePath.empty())
	{
		const std::filesystem::path firstComponent = *RelativePath.begin();
		if (firstComponent != ".." && firstComponent != ".")
			return false;
	}

	return true;
}

std::filesystem::path EditorLayer::GetRecentProjectsPath() const
{
	return std::filesystem::current_path() / "WhipHubRecentProjects.txt";
}

std::filesystem::path EditorLayer::GetPreferencesPath() const
{
	return std::filesystem::current_path() / "WhipEditorPreferences.yaml";
}

void EditorLayer::LoadEditorPreferences()
{
	LoadRecentProjects();

	const std::filesystem::path preferencesPath = GetPreferencesPath();
	std::error_code error;
	if (!std::filesystem::exists(preferencesPath, error))
		return;

	YAML::Node data;
	try
	{
		data = YAML::LoadFile(preferencesPath.string());
	}
	catch (const YAML::Exception& exception)
	{
		WHP_EDITOR_WARN(std::string("[Editor Preferences] Could not read preferences: ") + exception.what());
		return;
	}

	if (YAML::Node recentProjects = data["recentProjects"])
	{
		m_RecentProjects.clear();
		for (const YAML::Node& recentProject : recentProjects)
		{
			std::filesystem::path path = recentProject.as<std::string>("");
			if (!path.empty() && ShouldIncludeRecentProject(path))
				m_RecentProjects.push_back(path);
		}
	}

	m_LastProjectPath = data["last_project"].as<std::string>("");

	if (YAML::Node editor = data["editor"])
	{
		m_UISettings.SetShowPhysicsColliders(editor["show_physics_colliders"].as<bool>(m_UISettings.GetShowPhysicsColliders()));
		m_UISettings.SetStepFrame(editor["step_frame"].as<int>(m_UISettings.GetStepFrame()));
		m_UISettings.SetTheme(ThemeFromString(editor["theme"].as<std::string>(UI::UISettings::GetThemeName(m_UISettings.GetTheme()))));

		if (YAML::Node snap = editor["snap"])
		{
			m_UISettings.SetSnapValues(0, ReadVec3(snap["translation"], m_UISettings.GetSnapValues(0)));
			m_UISettings.SetSnapValues(1, ReadVec3(snap["rotation"], m_UISettings.GetSnapValues(1)));
			m_UISettings.SetSnapValues(2, ReadVec3(snap["scale"], m_UISettings.GetSnapValues(2)));
		}

		if (YAML::Node shortcuts = editor["shortcuts"])
		{
			for (size_t i = 0; i < UI::UISettings::ActionCount; ++i)
			{
				UI::EditorShortcutAction action = static_cast<UI::EditorShortcutAction>(i);
				YAML::Node shortcut = shortcuts[UI::UISettings::GetActionStorageKey(action)];
				if (!shortcut)
					continue;

				UI::ShortcutBinding binding;
				binding.m_Key = static_cast<KeyCode>(shortcut["key"].as<int>(0));
				binding.m_Ctrl = shortcut["ctrl"].as<bool>(false);
				binding.m_Shift = shortcut["shift"].as<bool>(false);
				binding.m_Alt = shortcut["alt"].as<bool>(false);
				m_UISettings.SetShortcutBinding(action, binding);
			}
		}
	}

	if (YAML::Node panels = data["panels"])
	{
		m_AnimationEditorPanel.SetOpen(panels["animation_editor"].as<bool>(m_AnimationEditorPanel.IsOpen()));
		m_SceneHierarchyPanel.SetOpen(panels["scene_hierarchy"].as<bool>(m_SceneHierarchyPanel.IsOpen()));
		m_UIStatistics.SetOpen(panels["statistics"].as<bool>(m_UIStatistics.IsOpen()));
		ConsolePanel::SetOpen(panels["console"].as<bool>(ConsolePanel::IsOpen()));
	}

	if (YAML::Node browser = data["content_browser"])
	{
		m_ContentBrowserPreferences.m_ThumbnailSize = browser["thumbnail_size"].as<float>(m_ContentBrowserPreferences.m_ThumbnailSize);
		m_ContentBrowserPreferences.m_Padding = browser["padding"].as<float>(m_ContentBrowserPreferences.m_Padding);
		m_ContentBrowserPreferences.m_ShowUnsupported = browser["show_unsupported"].as<bool>(m_ContentBrowserPreferences.m_ShowUnsupported);
		m_ContentBrowserPreferences.m_Open = browser["open"].as<bool>(m_ContentBrowserPreferences.m_Open);
		m_ContentBrowserPreferences.m_Mode = browser["mode"].as<int>(m_ContentBrowserPreferences.m_Mode);
		m_ContentBrowserPreferences.m_TypeFilter = browser["type_filter"].as<int>(m_ContentBrowserPreferences.m_TypeFilter);
		m_ContentBrowserPreferences.m_CurrentDirectory = browser["current_directory"].as<std::string>("");
		m_HasContentBrowserPreferences = true;
	}

	m_UISettings.ConsumeDirty();
	m_SceneHierarchyPanel.ConsumeOpenDirty();
	m_AnimationEditorPanel.ConsumeOpenDirty();
	m_UIStatistics.ConsumeOpenDirty();
	ConsolePanel::ConsumeOpenDirty();
	m_ProjectLoader.SetRecentProjects(m_RecentProjects);
}

void EditorLayer::SaveEditorPreferences() const
{
	YAML::Emitter out;
	out << YAML::BeginMap;
	out << YAML::Key << "last_project" << YAML::Value << m_LastProjectPath.string();
	out << YAML::Key << "recentProjects" << YAML::Value << YAML::BeginSeq;
	for (const auto& projectPath : m_RecentProjects)
		out << projectPath.string();
	out << YAML::EndSeq;

	out << YAML::Key << "editor" << YAML::Value << YAML::BeginMap;
	out << YAML::Key << "show_physics_colliders" << YAML::Value << m_UISettings.GetShowPhysicsColliders();
	out << YAML::Key << "step_frame" << YAML::Value << m_UISettings.GetStepFrame();
	out << YAML::Key << "theme" << YAML::Value << UI::UISettings::GetThemeName(m_UISettings.GetTheme());
	out << YAML::Key << "snap" << YAML::Value << YAML::BeginMap;
	out << YAML::Key << "translation" << YAML::Value; WriteVec3(out, m_UISettings.GetSnapValues(0));
	out << YAML::Key << "rotation" << YAML::Value; WriteVec3(out, m_UISettings.GetSnapValues(1));
	out << YAML::Key << "scale" << YAML::Value; WriteVec3(out, m_UISettings.GetSnapValues(2));
	out << YAML::EndMap;
	out << YAML::Key << "shortcuts" << YAML::Value << YAML::BeginMap;
	for (size_t i = 0; i < UI::UISettings::ActionCount; ++i)
	{
		UI::EditorShortcutAction action = static_cast<UI::EditorShortcutAction>(i);
		UI::ShortcutBinding binding = m_UISettings.GetShortcutBinding(action);
		out << YAML::Key << UI::UISettings::GetActionStorageKey(action) << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "key" << YAML::Value << binding.m_Key;
		out << YAML::Key << "ctrl" << YAML::Value << binding.m_Ctrl;
		out << YAML::Key << "shift" << YAML::Value << binding.m_Shift;
		out << YAML::Key << "alt" << YAML::Value << binding.m_Alt;
		out << YAML::EndMap;
	}
	out << YAML::EndMap;
	out << YAML::EndMap;

	out << YAML::Key << "panels" << YAML::Value << YAML::BeginMap;
	out << YAML::Key << "animation_editor" << YAML::Value << m_AnimationEditorPanel.IsOpen();
	out << YAML::Key << "scene_hierarchy" << YAML::Value << m_SceneHierarchyPanel.IsOpen();
	out << YAML::Key << "statistics" << YAML::Value << m_UIStatistics.IsOpen();
	out << YAML::Key << "console" << YAML::Value << ConsolePanel::IsOpen();
	out << YAML::EndMap;

	ContentBrowserPanel::Preferences browserPreferences = m_ContentBrowserPanel ? m_ContentBrowserPanel->GetPreferences() : m_ContentBrowserPreferences;
	out << YAML::Key << "content_browser" << YAML::Value << YAML::BeginMap;
	out << YAML::Key << "thumbnail_size" << YAML::Value << browserPreferences.m_ThumbnailSize;
	out << YAML::Key << "padding" << YAML::Value << browserPreferences.m_Padding;
	out << YAML::Key << "show_unsupported" << YAML::Value << browserPreferences.m_ShowUnsupported;
	out << YAML::Key << "open" << YAML::Value << browserPreferences.m_Open;
	out << YAML::Key << "mode" << YAML::Value << browserPreferences.m_Mode;
	out << YAML::Key << "type_filter" << YAML::Value << browserPreferences.m_TypeFilter;
	out << YAML::Key << "current_directory" << YAML::Value << browserPreferences.m_CurrentDirectory.string();
	out << YAML::EndMap;

	out << YAML::EndMap;

	std::ofstream stream(GetPreferencesPath(), std::ios::trunc);
	if (!stream)
		return;
	stream << out.c_str();
}

void EditorLayer::ApplyPreferencesToContentBrowser()
{
	if (m_ContentBrowserPanel && m_HasContentBrowserPreferences)
		m_ContentBrowserPanel->ApplyPreferences(m_ContentBrowserPreferences);
}

bool EditorLayer::NewProject(const UI::ProjectCreateSettings& settings)
{
	const std::string projectName = SanitizeProjectToken(settings.m_Name, "Untitled");
	const std::string projectFolderName = SanitizePathToken(projectName, "Untitled");
	const std::string initialSceneName = SanitizePathToken(settings.m_InitialSceneName, "Main");
	if (settings.m_Location.empty())
		return false;

	std::filesystem::path projectDirectory = settings.m_Location / projectFolderName;
	std::filesystem::path projectPath = projectDirectory / (projectFolderName + FileExtensions::Project);
	std::error_code error;
	if (std::filesystem::exists(projectPath, error))
	{
		WHP_EDITOR_WARN(std::string("[Whip Hub] Project file already exists: ") + projectPath.string());
		return false;
	}

	if (!CreateDirectoryChecked(projectDirectory / "Assets" / "Scenes", "Project scenes directory") ||
		!CreateDirectoryChecked(projectDirectory / "Assets" / "Scripts" / "Source", "script source directory") ||
		!CreateDirectoryChecked(projectDirectory / "Assets" / "Scripts" / "Binaries", "script binaries directory") ||
		!CreateDirectoryChecked(projectDirectory / "Assets" / "Animations", "animations directory") ||
		!CreateDirectoryChecked(projectDirectory / "Assets" / "Audios", "audio directory") ||
		!CreateDirectoryChecked(projectDirectory / "Assets" / "fonts", "font directory") ||
		!CreateDirectoryChecked(projectDirectory / "Assets" / "textures", "texture directory"))
	{
		return false;
	}

	Ref<Project> newProject = Project::NewProject();
	Project::SetActiveProjectPath(projectPath);

	ProjectConfig& config = newProject->GetConfig();
	config.m_Name = projectName;
	config.m_AssetDirectory = "Assets";
	config.m_AssetRegistryPath = FileExtensions::AssetRegistryFilename;
	config.m_ScriptModulePath = std::filesystem::path("Scripts") / "Binaries" / (projectFolderName + ".dll");
	config.m_StartScene = 0;

	if (!WriteScriptProjectFiles(projectDirectory, projectFolderName))
	{
		Project::SetActive(nullptr);
		return false;
	}

	std::filesystem::path startSceneRelativePath;
	AssetHandle startSceneHandle = 0;
	if (settings.m_CreateStartScene)
	{
		startSceneHandle = AssetHandle{};
		config.m_StartScene = startSceneHandle;
		startSceneRelativePath = std::filesystem::path("Scenes") / (initialSceneName + FileExtensions::Scene);

	Ref<Scene> startScene = MakeRef<Scene>(startSceneHandle);
		if (settings.m_TemplateIndex == 1 || settings.m_TemplateIndex == 2)
		{
			Entity camera = startScene->CreateEntity("Main Camera");
			camera.AddComponent<CameraComponent>();
			camera.GetComponent<TransformComponent>().m_Translation = { 0.0f, 0.0f, 8.0f };

			Entity sprite = startScene->CreateEntity(settings.m_TemplateIndex == 2 ? "Starter Entity" : "Sprite");
			sprite.AddComponent<SpriteRendererComponent>(glm::vec4{ 0.86f, 0.58f, 0.28f, 1.0f });
			if (settings.m_TemplateIndex == 2)
				sprite.AddComponent<ScriptComponent>().m_ClassName = projectFolderName + ".StarterEntity";
		}

		SceneImporter::SaveScene(startScene, startSceneRelativePath);
	}

	if (!Project::SaveActive(projectPath))
	{
		Project::SetActive(nullptr);
		return false;
	}

	std::ofstream registry(projectPath.parent_path() / config.m_AssetDirectory / config.m_AssetRegistryPath, std::ios::trunc);
	if (!registry)
	{
		WHP_EDITOR_ERROR("[Whip Hub] Could not write Asset registry.");
		Project::SetActive(nullptr);
		return false;
	}

	if (settings.m_CreateStartScene)
	{
		registry << "AssetRegistry:\n";
		registry << "  - handle: " << (uint64_t)startSceneHandle << '\n';
		registry << "    filepath: " << startSceneRelativePath.generic_string() << '\n';
		registry << "    type: scene\n";
	}
	else
	{
		registry << "AssetRegistry: []\n";
	}
	registry.close();

	Project::SetActive(nullptr);
	if (!settings.m_OpenAfterCreate)
	{
		AddRecentProject(projectPath);
		return true;
	}
	return OpenProject(projectPath);
}

void EditorLayer::SaveProject()
{
	if (!HasProjectLoaded())
		return;

	Project::SaveActive();
}

void EditorLayer::FinishProjectSettings()
{
	if (!HasProjectLoaded())
		return;

	Project::SaveActive();
	ReloadAssembly(true);
	m_ContentBrowserPanel = MakeScope<ContentBrowserPanel>(Project::GetActive());
	m_ContentBrowserPanel->SetAssetOpenCallback([this](AssetHandle handle) { return HandleContentBrowserAssetOpen(handle); });
	m_ContentBrowserPanel->SetAssetInspectCallback([this](AssetHandle handle) { return HandleContentBrowserAssetInspect(handle); });
	ApplyPreferencesToContentBrowser();
}

void EditorLayer::MigrateProjectNativeFileExtensions()
{
	if (!HasProjectLoaded())
		return;

	Ref<EditorAssetManager> AssetManager = Project::GetActive()->GetEditorAssetManager();
	if (!AssetManager)
		return;

	std::vector<std::pair<AssetHandle, std::filesystem::path>> scenePaths;
	const auto& scenes = AssetManager->GetAssetRegistry().GetFiltered(AssetType::Scene);
	scenePaths.reserve(scenes.size());
	for (const auto& [handle, metadata] : scenes)
	{
		if (FileExtensions::IsLegacySceneExtension(metadata.m_Filepath))
			scenePaths.emplace_back(handle, metadata.m_Filepath);
	}

	size_t migratedCount = 0;
	const std::filesystem::path assetDirectory = Project::GetActiveAssetDirectory();
	for (const auto& [handle, legacyRelativePath] : scenePaths)
	{
		std::filesystem::path modernRelativePath = legacyRelativePath;
		modernRelativePath.replace_extension(FileExtensions::Scene);

		const std::filesystem::path legacyPath = assetDirectory / legacyRelativePath;
		const std::filesystem::path modernPath = assetDirectory / modernRelativePath;

		std::error_code error;
		const bool modernExists = std::filesystem::exists(modernPath, error);
		error.clear();
		const bool legacyExists = std::filesystem::exists(legacyPath, error);
		if (!modernExists && legacyExists)
		{
			error.clear();
			std::filesystem::rename(legacyPath, modernPath, error);
			if (error)
			{
				WHP_EDITOR_WARN(std::string("[Project] Could not migrate scene extension '") +
					legacyPath.string() + "' -> '" + modernPath.string() + "': " + error.message());
				continue;
			}
		}
		else if (!modernExists)
		{
			continue;
		}

		if (AssetManager->UpdateAssetFilepath(handle, modernRelativePath))
			++migratedCount;
	}

	if (migratedCount > 0)
		WHP_EDITOR_INFO(std::string("[Project] Migrated ") + std::to_string(migratedCount) + " scene file extension(s) to .wscene.");
}


bool EditorLayer::OpenProject()
{
	std::string filepath = FileDialogs::OpenFile("Whip Project (*.wproj)\0*.wproj\0");
	if (filepath.empty())
		return false;
	return OpenProject(filepath);
}

bool EditorLayer::OpenProject(const std::filesystem::path& path)
{
	if (path.empty())
		return false;

	std::filesystem::path projectPath = NormalizeProjectListPath(path);
	std::error_code error;
	if (!std::filesystem::exists(projectPath, error) || !FileExtensions::IsProjectExtension(projectPath))
	{
		WHP_EDITOR_WARN(std::string("[Project] Project file is missing or invalid: ") + projectPath.string());
		m_ProjectLoader.SetStatus("Project file is missing or invalid.");
		return false;
	}

	if (HasProjectLoaded() && PathsMatchForRecentProject(Project::GetActive()->GetProjectPath(), projectPath))
	{
		WHP_EDITOR_INFO(std::string("[Project] Project is already open: ") + projectPath.string());
		AddRecentProject(projectPath);
		m_ProjectLoader.SetLoaded(true);
		m_ProjectLoader.SetStatus("Project already open.");
		return true;
	}

	WHP_EDITOR_INFO(std::string("[Project] Opening Project: ") + projectPath.string());
	if (HasProjectLoaded())
	{
		WHP_EDITOR_INFO("[Project] Unloading current Project before opening a new one.");
		ResetEditorProjectState();
	}

	if (Project::Load(projectPath))
	{
		WHP_EDITOR_INFO("[Project] Project file loaded.");
		MigrateProjectNativeFileExtensions();
		WHP_EDITOR_INFO("[Project] Native file extension migration complete.");
		const bool scriptBuildSucceeded = BuildProjectScripts();
		if (!scriptBuildSucceeded)
			WHP_EDITOR_WARN("[Script Build] Project opened, but script build failed.");
		WHP_EDITOR_INFO("[Project] Script build step complete.");
		ScriptEngine::Init();
		WHP_EDITOR_INFO("[Project] Script engine initialized.");
		StartScriptSourceWatcher();
		AssetHandle startScene = (Project::GetActive()->GetConfig().m_StartScene);
		if (startScene && Project::GetActive()->GetEditorAssetManager()->IsAssetHandleValid(startScene))
		{
			const std::filesystem::path startScenePath = Project::GetActive()->GetEditorAssetManager()->GetFilepath(startScene);
			if (std::filesystem::exists(Project::GetActiveAssetDirectory() / startScenePath))
				OpenScene(startScene);
			else
			{
				WHP_EDITOR_WARN("[Project] Start scene file is missing. Resetting Project start scene.");
				Project::GetActive()->GetConfig().m_StartScene = 0;
				Project::SaveActive();
			}
		}
		else if (startScene)
		{
			WHP_EDITOR_WARN("[Project] Start scene is missing. Resetting Project start scene.");
			Project::GetActive()->GetConfig().m_StartScene = 0;
			Project::SaveActive();
		}
		else
		{
			NewScene();
		}
		m_ContentBrowserPanel = MakeScope<ContentBrowserPanel>(Project::GetActive());
		m_ContentBrowserPanel->SetAssetOpenCallback([this](AssetHandle handle) { return HandleContentBrowserAssetOpen(handle); });
		m_ContentBrowserPanel->SetAssetInspectCallback([this](AssetHandle handle) { return HandleContentBrowserAssetInspect(handle); });
		ApplyPreferencesToContentBrowser();
		AddRecentProject(projectPath);
		m_ProjectLoader.SetLoaded(true);
		m_ProjectLoader.SetStatus(scriptBuildSucceeded ? "Project opened." : "Project opened, script build failed.");
		WHP_EDITOR_INFO("[Project] Project open complete.");
		return true;
	}
	ResetEditorProjectState();
	WHP_EDITOR_WARN(std::string("[Project] Project load failed: ") + projectPath.string());
	m_ProjectLoader.SetStatus("Project could not be opened.");
	return false;
}

void EditorLayer::ResetEditorProjectState()
{
	WriteSceneRecoverySnapshot("Project switch");
	StopScriptSourceWatcher();
	if (m_SceneState != SceneState::Edit)
		OnSceneStop();

	ScriptEngine::ClearRuntimeSceneTransitionRequest();
	{
		std::scoped_lock lock(m_ScriptSourceMutex);
		m_ScriptSourceDirty = false;
		m_ScriptSourceQueuedWhileRunning = false;
		m_LastScriptSourceChangePath.clear();
		m_LastScriptSourceChangeEvent.clear();
	}

	m_ContentBrowserPanel.reset();
	m_SceneHierarchyPanel.SetContext({});
	m_EditorScene = MakeRef<Scene>();
	m_ActiveScene = m_EditorScene;
	m_EditorScenePath.clear();
	ClearSceneHistory();
	MarkSceneClean();
	Project::SetActive(nullptr);
	m_ProjectLoader.SetLoaded(false);
	SetScriptBuildStatus("Scripts idle");
}

void EditorLayer::NewScene()
{
	if (!HasProjectLoaded())
		return;

    m_ActiveScene = MakeRef<Scene>();
	m_SceneHierarchyPanel.SetContext(m_ActiveScene);
	m_EditorScenePath = std::filesystem::path();
	ClearSceneHistory();
	MarkSceneClean();
}

void EditorLayer::OpenScene(AssetHandle handle)
{
	if (!HasProjectLoaded())
		return;

	if (m_SceneState != SceneState::Edit)
		OnSceneStop();

	if (!Project::GetActive()->GetEditorAssetManager()->IsAssetHandleValid(handle))
	{
		WHP_EDITOR_WARN("[Scene] Failed to open scene. Asset handle is not registered.");
		return;
	}
	const std::filesystem::path scenePath = Project::GetActive()->GetEditorAssetManager()->GetFilepath(handle);
	if (!std::filesystem::exists(Project::GetActiveAssetDirectory() / scenePath))
	{
		WHP_EDITOR_WARN(std::string("[Scene] Failed to open scene. File is missing: ") + scenePath.string());
		return;
	}

	Ref<Scene> readOnlyScene = AssetManager::GetAsset<Scene>(handle);
	if (!readOnlyScene)
	{
		WHP_EDITOR_WARN("[Scene] Failed to open scene. Asset is missing or failed to import.");
		return;
	}
	Ref<Scene> NewScene = Scene::Copy(readOnlyScene);

	m_EditorScene = NewScene;
	m_SceneHierarchyPanel.SetContext(m_EditorScene);

	m_ActiveScene = m_EditorScene;
	m_EditorScenePath = scenePath;
	ClearSceneHistory();
	MarkSceneClean();
}

void EditorLayer::CloseScene()
{
	if (m_SceneState != SceneState::Edit)
		OnSceneStop();
	Ref<Scene> NewScene = MakeRef<Scene>();
	m_EditorScene = NewScene;
	m_EditorScene->OnViewportResize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
	m_ActiveScene = m_EditorScene;
	m_EditorScenePath.clear();
	m_SceneHierarchyPanel.SetContext({});
	ClearSceneHistory();
	MarkSceneClean();
}

void EditorLayer::SaveScene()
{
	if (!HasProjectLoaded())
		return;

	if (!m_EditorScenePath.empty())
	{
		SerializeScene(m_ActiveScene, m_EditorScenePath);
		MarkSceneClean();
	}
	else
		SaveSceneAs();
}

void EditorLayer::SaveSceneAs()
{
	if (!HasProjectLoaded())
		return;

	const std::filesystem::path scenesDirectory = Project::GetActiveAssetDirectory() / "Scenes";
	std::error_code error;
	std::filesystem::create_directories(scenesDirectory, error);

    std::string filepath = FileDialogs::SaveFile("Whip Scene (*.wscene)\0*.wscene\0", scenesDirectory.string().c_str());
	if (filepath.empty())
		return;

	std::filesystem::path scenePath(filepath);
	if (!FileExtensions::IsSceneExtension(scenePath))
		scenePath.replace_extension(FileExtensions::Scene);
	else if (FileExtensions::IsLegacySceneExtension(scenePath))
		scenePath.replace_extension(FileExtensions::Scene);

	SerializeScene(m_ActiveScene, scenePath);

	const std::filesystem::path assetDirectory = Project::GetActiveAssetDirectory();
	if (PathIsOrIsUnder(scenePath, assetDirectory))
	{
		error.clear();
		const std::filesystem::path RelativePath = std::filesystem::relative(scenePath, assetDirectory, error).lexically_normal();
		if (!error)
		{
			m_EditorScenePath = RelativePath;
			AssetHandle handle = Project::GetActive()->GetEditorAssetManager()->GetHandleFromFilepath(RelativePath);
			if (handle == 0)
				handle = Project::GetActive()->GetEditorAssetManager()->ImportAsset(RelativePath);
			if (handle != 0)
			{
				m_ActiveScene->m_Handle = handle;
				if (m_EditorScene)
					m_EditorScene->m_Handle = handle;
			}
		}
		else
		{
			m_EditorScenePath = scenePath;
		}
	}
	else
	{
		m_EditorScenePath = scenePath;
	}

	if (m_ContentBrowserPanel)
		m_ContentBrowserPanel->RefreshAssetTree();
	MarkSceneClean();
}

void EditorLayer::MarkSceneDirty()
{
	if (m_SceneState == SceneState::Edit && m_EditorScene)
		m_SceneDirty = true;
}

void EditorLayer::MarkSceneClean()
{
	m_SceneDirty = false;
}

std::filesystem::path EditorLayer::GetSceneRecoveryPath() const
{
	Ref<Project> activeProject = Project::GetActive();
	if (!activeProject)
		return {};

	std::filesystem::path recoveryDirectory = activeProject->GetProjectDirectory() / ".whip_recovery";
	std::string sceneName = m_EditorScenePath.empty() ? "Untitled" : m_EditorScenePath.filename().stem().string();
	if (sceneName.empty())
		sceneName = "Untitled";

	return recoveryDirectory / (sceneName + ".recovery" + FileExtensions::Scene);
}

void EditorLayer::WriteSceneRecoverySnapshot(const char* reason)
{
	if (!HasProjectLoaded() || !m_EditorScene || !m_SceneDirty || m_SceneState != SceneState::Edit)
		return;

	const std::filesystem::path recoveryPath = GetSceneRecoveryPath();
	if (recoveryPath.empty())
		return;

	std::error_code error;
	std::filesystem::create_directories(recoveryPath.parent_path(), error);
	if (error)
	{
		WHP_EDITOR_WARN(std::string("[Scene Recovery] Could not create recovery directory: ") + error.message());
		return;
	}

	SceneImporter::SaveScene(m_EditorScene, recoveryPath);
	m_LastSceneRecoverySnapshot = std::chrono::steady_clock::now();
	WHP_EDITOR_INFO(std::string("[Scene Recovery] Snapshot written (") + reason + "): " + recoveryPath.string());
}

void EditorLayer::SaveEntityTemplate(Entity entityIn)
{
	if (!HasProjectLoaded() || !entityIn)
		return;

	const std::filesystem::path templatesDirectory = Project::GetActiveAssetDirectory() / "EntityTemplates";
	std::error_code error;
	std::filesystem::create_directories(templatesDirectory, error);

	std::string filepath = FileDialogs::SaveFile("Whip Entity Template (*.went)\0*.went\0", templatesDirectory.string().c_str());
	if (filepath.empty())
		return;

	std::filesystem::path templatePath(filepath);
	if (!FileExtensions::IsEntityTemplateExtension(templatePath))
		templatePath.replace_extension(FileExtensions::EntityTemplate);

	SceneSerializer serializer(m_EditorScene);
	if (!serializer.SerializeEntityTemplate(entityIn, templatePath))
	{
		WHP_EDITOR_ERROR(std::string("[Entity Template] Could not save template: ") + templatePath.string());
		return;
	}

	const std::filesystem::path assetDirectory = Project::GetActiveAssetDirectory();
	if (PathIsOrIsUnder(templatePath, assetDirectory))
	{
		error.clear();
		const std::filesystem::path RelativePath = std::filesystem::relative(templatePath, assetDirectory, error).lexically_normal();
		if (!error)
			Project::GetActive()->GetEditorAssetManager()->ImportAsset(RelativePath);
	}

	if (m_ContentBrowserPanel)
		m_ContentBrowserPanel->RefreshAssetTree();

	WHP_EDITOR_INFO(std::string("[Entity Template] Saved ") + templatePath.string());
}

bool EditorLayer::InstantiateEntityTemplate(AssetHandle handle)
{
	if (!HasProjectLoaded() || !m_EditorScene)
		return false;

	Ref<Project> activeProject = Project::GetActive();
	if (!activeProject->GetEditorAssetManager()->IsAssetHandleValid(handle) ||
		activeProject->GetEditorAssetManager()->GetAssetType(handle) != AssetType::Entity)
	{
		return false;
	}

	if (m_SceneState != SceneState::Edit)
	{
		WHP_EDITOR_WARN("[Entity Template] Templates can only be instantiated while editing.");
		return false;
	}

	const std::filesystem::path templatePath = Project::GetActiveAssetDirectory() / activeProject->GetEditorAssetManager()->GetFilepath(handle);
	CaptureSceneHistory();
	SceneSerializer serializer(m_EditorScene);
	Entity instance = serializer.DeserializeEntityTemplate(templatePath, handle);
	if (!instance)
	{
		WHP_EDITOR_ERROR(std::string("[Entity Template] Could not instantiate template: ") + templatePath.string());
		return false;
	}

	m_SceneHierarchyPanel.SetSelectedEntity(instance);
	WHP_EDITOR_INFO(std::string("[Entity Template] Instantiated ") + templatePath.string());
	return true;
}

Entity EditorLayer::FindPrefabRoot(Entity entityIn) const
{
	if (!entityIn || !entityIn.HasComponent<PrefabComponent>())
		return {};

	const AssetHandle source = entityIn.GetComponent<PrefabComponent>().m_Source;
	Entity current = entityIn;
	while (current && current.HasComponent<HierarchyComponent>())
	{
		if (current.HasComponent<PrefabComponent>())
		{
			const auto& prefab = current.GetComponent<PrefabComponent>();
			if (prefab.m_Source == source && prefab.m_Root)
				return current;
		}

		const auto& hierarchy = current.GetComponent<HierarchyComponent>();
		if (hierarchy.m_Parent == 0)
			break;

		current = m_EditorScene ? m_EditorScene->FindEntityByUUID(hierarchy.m_Parent) : Entity{};
	}

	return entityIn.GetComponent<PrefabComponent>().m_Root ? entityIn : Entity{};
}

void EditorLayer::RemovePrefabLinksRecursive(Entity entityIn)
{
	if (!entityIn)
		return;

	std::vector<UUID> children;
	if (entityIn.HasComponent<HierarchyComponent>())
		children = entityIn.GetComponent<HierarchyComponent>().m_Children;

	if (entityIn.HasComponent<PrefabComponent>())
		entityIn.RemoveComponent<PrefabComponent>();

	for (UUID childId : children)
	{
		Entity child = m_EditorScene ? m_EditorScene->FindEntityByUUID(childId) : Entity{};
		if (child)
			RemovePrefabLinksRecursive(child);
	}
}

void EditorLayer::ApplyEntityTemplate(Entity entityIn)
{
	if (!HasProjectLoaded() || !m_EditorScene)
		return;

	Entity root = FindPrefabRoot(entityIn);
	if (!root || !root.HasComponent<PrefabComponent>())
	{
		WHP_EDITOR_WARN("[Entity Template] Apply failed. Select a template instance root or child.");
		return;
	}

	Ref<Project> activeProject = Project::GetActive();
	AssetHandle handle = root.GetComponent<PrefabComponent>().m_Source;
	if (!activeProject->GetEditorAssetManager()->IsAssetHandleValid(handle) ||
		activeProject->GetEditorAssetManager()->GetAssetType(handle) != AssetType::Entity)
	{
		WHP_EDITOR_WARN("[Entity Template] Apply failed. The source template Asset is missing.");
		return;
	}

	const std::filesystem::path templatePath = Project::GetActiveAssetDirectory() / activeProject->GetEditorAssetManager()->GetFilepath(handle);
	std::error_code error;
	if (!std::filesystem::exists(templatePath, error))
	{
		WHP_EDITOR_WARN(std::string("[Entity Template] Apply failed. File is missing: ") + templatePath.string());
		return;
	}

	SceneSerializer serializer(m_EditorScene);
	if (!serializer.SerializeEntityTemplate(root, templatePath))
	{
		WHP_EDITOR_ERROR(std::string("[Entity Template] Could not apply instance to template: ") + templatePath.string());
		return;
	}

	activeProject->GetEditorAssetManager()->UnloadAsset(handle);
	if (m_ContentBrowserPanel)
		m_ContentBrowserPanel->RefreshAssetTree();

	WHP_EDITOR_INFO(std::string("[Entity Template] Applied instance to ") + templatePath.string());
}

void EditorLayer::RevertEntityTemplate(Entity entityIn)
{
	if (!HasProjectLoaded() || !m_EditorScene)
		return;

	Entity root = FindPrefabRoot(entityIn);
	if (!root || !root.HasComponent<PrefabComponent>())
	{
		WHP_EDITOR_WARN("[Entity Template] Revert failed. Select a template instance root or child.");
		return;
	}

	Ref<Project> activeProject = Project::GetActive();
	AssetHandle handle = root.GetComponent<PrefabComponent>().m_Source;
	if (!activeProject->GetEditorAssetManager()->IsAssetHandleValid(handle) ||
		activeProject->GetEditorAssetManager()->GetAssetType(handle) != AssetType::Entity)
	{
		WHP_EDITOR_WARN("[Entity Template] Revert failed. The source template Asset is missing.");
		return;
	}

	const std::filesystem::path templatePath = Project::GetActiveAssetDirectory() / activeProject->GetEditorAssetManager()->GetFilepath(handle);
	std::error_code error;
	if (!std::filesystem::exists(templatePath, error))
	{
		WHP_EDITOR_WARN(std::string("[Entity Template] Revert failed. File is missing: ") + templatePath.string());
		return;
	}

	{
		Ref<Scene> validationScene = MakeRef<Scene>();
		SceneSerializer validator(validationScene);
		if (!validator.DeserializeEntityTemplate(templatePath, handle))
		{
			WHP_EDITOR_ERROR(std::string("[Entity Template] Revert failed. Could not read template: ") + templatePath.string());
			return;
		}
	}

	UUID parentId = 0;
	size_t childIndex = 0;
	bool hadChildIndex = false;
	if (root.HasComponent<HierarchyComponent>())
	{
		const auto& hierarchy = root.GetComponent<HierarchyComponent>();
		parentId = hierarchy.m_Parent;
		if (parentId != 0)
		{
			Entity parent = m_EditorScene->FindEntityByUUID(parentId);
			if (parent && parent.HasComponent<HierarchyComponent>())
			{
				const auto& siblings = parent.GetComponent<HierarchyComponent>().m_Children;
				auto siblingIt = std::find(siblings.begin(), siblings.end(), root.GetUUID());
				if (siblingIt != siblings.end())
				{
					childIndex = static_cast<size_t>(std::distance(siblings.begin(), siblingIt));
					hadChildIndex = true;
				}
			}
		}
	}

	TransformComponent preservedTransform{};
	if (root.HasComponent<TransformComponent>())
		preservedTransform = root.GetComponent<TransformComponent>();

	CaptureSceneHistory();
	m_EditorScene->DestroyEntity(root);

	SceneSerializer serializer(m_EditorScene);
	Entity reverted = serializer.DeserializeEntityTemplate(templatePath, handle);
	if (!reverted)
	{
		WHP_EDITOR_ERROR(std::string("[Entity Template] Revert failed after validation: ") + templatePath.string());
		return;
	}

	if (reverted.HasComponent<TransformComponent>())
		reverted.GetComponent<TransformComponent>() = preservedTransform;

	if (parentId != 0 && reverted.HasComponent<HierarchyComponent>())
	{
		Entity parent = m_EditorScene->FindEntityByUUID(parentId);
		if (parent && parent.HasComponent<HierarchyComponent>())
		{
			auto& hierarchy = reverted.GetComponent<HierarchyComponent>();
			hierarchy.m_Parent = parentId;

			auto& siblings = parent.GetComponent<HierarchyComponent>().m_Children;
			siblings.erase(std::remove(siblings.begin(), siblings.end(), reverted.GetUUID()), siblings.end());
			size_t insertIndex = hadChildIndex ? std::min(childIndex, siblings.size()) : siblings.size();
			siblings.insert(siblings.begin() + static_cast<std::vector<UUID>::difference_type>(insertIndex), reverted.GetUUID());
		}
	}

	m_SceneHierarchyPanel.SetSelectedEntity(reverted);
	WHP_EDITOR_INFO(std::string("[Entity Template] Reverted instance from ") + templatePath.string());
}

void EditorLayer::UnpackEntityTemplate(Entity entityIn)
{
	if (!HasProjectLoaded() || !m_EditorScene)
		return;

	Entity root = FindPrefabRoot(entityIn);
	if (!root)
	{
		WHP_EDITOR_WARN("[Entity Template] Unpack failed. Select a template instance root or child.");
		return;
	}

	CaptureSceneHistory();
	RemovePrefabLinksRecursive(root);
	m_SceneHierarchyPanel.SetSelectedEntity(root);
	WHP_EDITOR_INFO(std::string("[Entity Template] Unpacked instance ") + root.GetName());
}

bool EditorLayer::BuildProjectScripts()
{
	if (!HasProjectLoaded())
		return false;

	const ProjectConfig& config = Project::GetActive()->GetConfig();
	if (config.m_ScriptModulePath.empty())
	{
		WHP_EDITOR_INFO("[Script Build] Project has no script module configured.");
		SetScriptBuildStatus("No script module", true);
		return true;
	}

	const std::filesystem::path scriptsDirectory = Project::GetActiveAssetDirectory() / "Scripts";
	const std::string scriptWorkspaceName = GetScriptWorkspaceName(config);
	const std::filesystem::path preferredProjectFile = scriptsDirectory / (scriptWorkspaceName + ".csproj");
	const std::filesystem::path preferredSolutionFile = scriptsDirectory / (scriptWorkspaceName + ".sln");
	const std::filesystem::path buildPropsFile = scriptsDirectory / "Directory.Build.props";
	std::error_code error;
	const bool preferredProjectExists = std::filesystem::exists(preferredProjectFile, error);
	error.clear();
	const bool preferredSolutionExists = std::filesystem::exists(preferredSolutionFile, error);
	error.clear();
	const bool buildPropsExists = std::filesystem::exists(buildPropsFile, error);
	bool needsWorkspaceRefresh = !preferredProjectExists || !preferredSolutionExists;
	if (!needsWorkspaceRefresh && preferredProjectExists)
	{
		const std::string projectFileContents = ReadTextFile(preferredProjectFile);
		const bool generatedSdkProject = projectFileContents.find("<Project Sdk=\"Microsoft.NET.Sdk\">") != std::string::npos;
		if (generatedSdkProject)
		{
			const std::string solutionFileContents = ReadTextFile(preferredSolutionFile);
			const std::string buildPropsContents = buildPropsExists ? ReadTextFile(buildPropsFile) : "";
			needsWorkspaceRefresh =
				!buildPropsExists ||
				projectFileContents.find("<ProjectReference Include=\"Whip-ScriptCore\\Whip-ScriptCore.csproj\">") == std::string::npos ||
				projectFileContents.find("<Compile Remove=\"Whip-ScriptCore\\**\\*.cs\" />") == std::string::npos ||
				projectFileContents.find("<Compile Remove=\"Intermediates\\**\\*.cs\" />") == std::string::npos ||
				projectFileContents.find("<Compile Remove=\"obj\\**\\*.cs\" />") == std::string::npos ||
				projectFileContents.find("<BaseIntermediateOutputPath>") != std::string::npos ||
				buildPropsContents.find("ScriptIntermediates") == std::string::npos ||
				solutionFileContents.find("Whip-ScriptCore\\Whip-ScriptCore.csproj") == std::string::npos;
		}
	}
	if (needsWorkspaceRefresh)
	{
		WHP_EDITOR_INFO("[Script Build] Refreshing generated C# workspace files.");
		if (!RefreshScriptWorkspaceFiles(scriptsDirectory, scriptWorkspaceName))
			WHP_EDITOR_WARN("[Script Build] Could not refresh generated C# workspace files.");
	}

	SyncScriptCoreBinary(scriptsDirectory);

	const std::filesystem::path scriptProjectFile = FindScriptProjectFile(scriptsDirectory, scriptWorkspaceName);
	if (scriptProjectFile.empty())
	{
		WHP_EDITOR_WARN(std::string("[Script Build] No C# Project file found under ") + scriptsDirectory.string() + ". Reloading the existing assembly if it exists.");
		SetScriptBuildStatus("No C# Project found", true);
		return true;
	}

	const ScriptBuildCommand buildCommand = MakeScriptBuildCommand(scriptProjectFile);
	if (buildCommand.command.empty())
	{
		WHP_EDITOR_WARN("[Script Build] Could not find MSBuild.exe or dotnet. Set WHIP_MSBUILD_PATH or add MSBuild/dotnet to PATH. Reloading the existing assembly if it exists.");
		SetScriptBuildStatus("MSBuild/dotnet not found", true, true);
		return true;
	}

	WHP_EDITOR_INFO(std::string("[Script Build] Building ") + scriptProjectFile.string() + " with " + buildCommand.toolName);
	SetScriptBuildStatus("Building scripts...");
	const int result = RunCommandAndLogOutput(buildCommand.command);
	if (result != 0)
	{
		WHP_EDITOR_ERROR(std::string("[Script Build] Build failed with exit code ") + std::to_string(result) + ".");
		SetScriptBuildStatus("Script build failed", false, true);
		return false;
	}

	WHP_EDITOR_INFO("[Script Build] Build succeeded.");
	SetScriptBuildStatus("Scripts built");
	return true;
}

void EditorLayer::SetScriptBuildStatus(const std::string& message, bool warning, bool failure)
{
	m_ScriptBuildStatus = message;
	m_ScriptBuildStatusTime = std::chrono::steady_clock::now();
	m_ScriptBuildStatusWarning = warning;
	m_ScriptBuildStatusFailure = failure;
}

void EditorLayer::ProcessRuntimeSceneTransition()
{
	if (m_SceneState != SceneState::Play && m_SceneState != SceneState::Simulate)
	{
		ScriptEngine::ClearRuntimeSceneTransitionRequest();
		return;
	}

	const RuntimeSceneTransitionRequest request = ScriptEngine::ConsumeRuntimeSceneTransitionRequest();
	switch (request.m_Type)
	{
	case RuntimeSceneTransitionType::Load:
	case RuntimeSceneTransitionType::Reload:
		LoadRuntimeScene(request.m_SceneHandle);
		break;
	case RuntimeSceneTransitionType::Unload:
		UnloadRuntimeScene();
		break;
	default:
		break;
	}
}

bool EditorLayer::LoadRuntimeScene(AssetHandle handle)
{
	if (!HasProjectLoaded() || handle == 0)
		return false;

	Ref<Project> activeProject = Project::GetActive();
	if (!activeProject->GetRuntimeAssetManager()->IsAssetHandleValid(handle) ||
		activeProject->GetRuntimeAssetManager()->GetAssetType(handle) != AssetType::Scene)
	{
		WHP_EDITOR_WARN("[Scene Manager] Runtime scene load failed. Invalid scene handle.");
		return false;
	}

	Ref<Scene> sourceScene = AssetManager::GetAsset<Scene>(handle);
	if (!sourceScene)
	{
		WHP_EDITOR_WARN("[Scene Manager] Runtime scene load failed. Scene Asset could not be loaded.");
		return false;
	}

	StopActiveRuntimeSceneForTransition();
	m_ActiveScene = Scene::Copy(sourceScene);
	m_ActiveScene->OnViewportResize(static_cast<uint32_t>(m_ViewportSize.x), static_cast<uint32_t>(m_ViewportSize.y));
	m_SceneHierarchyPanel.SetContext(m_ActiveScene);
	m_SceneHierarchyPanel.SetSelectedEntity({});
	StartActiveRuntimeSceneForTransition(handle);
	WHP_EDITOR_INFO(std::string("[Scene Manager] Runtime scene loaded: ") + activeProject->GetRuntimeAssetManager()->GetFilepath(handle).generic_string());
	return true;
}

bool EditorLayer::UnloadRuntimeScene()
{
	if (m_SceneState != SceneState::Play && m_SceneState != SceneState::Simulate)
		return false;

	StopActiveRuntimeSceneForTransition();
	m_ActiveScene = MakeRef<Scene>();
	m_ActiveScene->OnViewportResize(static_cast<uint32_t>(m_ViewportSize.x), static_cast<uint32_t>(m_ViewportSize.y));
	m_SceneHierarchyPanel.SetContext(m_ActiveScene);
	m_SceneHierarchyPanel.SetSelectedEntity({});
	StartActiveRuntimeSceneForTransition(0);
	WHP_EDITOR_INFO("[Scene Manager] Runtime scene unloaded.");
	return true;
}

void EditorLayer::StopActiveRuntimeSceneForTransition()
{
	if (!m_ActiveScene)
		return;

	if (m_SceneState == SceneState::Simulate)
		m_ActiveScene->OnSimulationStop();
	else
		m_ActiveScene->OnRuntimeStop();
}

void EditorLayer::StartActiveRuntimeSceneForTransition(AssetHandle handle)
{
	ScriptEngine::SetRuntimeActiveSceneHandle(handle);
	if (m_SceneState == SceneState::Simulate)
		m_ActiveScene->OnSimulationStart();
	else
		m_ActiveScene->OnRuntimeStart();
	ScriptEngine::SetRuntimeActiveSceneHandle(handle);
}

void EditorLayer::StartScriptSourceWatcher()
{
	StopScriptSourceWatcher();

	if (!HasProjectLoaded())
		return;

	const ProjectConfig& config = Project::GetActive()->GetConfig();
	if (config.m_ScriptModulePath.empty())
		return;

	const std::filesystem::path scriptsDirectory = Project::GetActiveAssetDirectory() / "Scripts";
	std::error_code error;
	if (!std::filesystem::exists(scriptsDirectory, error) || !std::filesystem::is_directory(scriptsDirectory, error))
		return;

	try
	{
		m_ScriptSourceWatchDirectory = scriptsDirectory;
		m_ScriptSourceWatcher = MakeScope<filewatch::FileWatch<std::string>>(
			scriptsDirectory.string(),
			[this](const std::string& path, const filewatch::Event eventType)
			{
				HandleScriptSourceEvent(path, eventType);
			});
		WHP_EDITOR_INFO(std::string("[Script Watcher] Watching C# source changes under ") + scriptsDirectory.string());
	}
	catch (const std::exception& exception)
	{
		m_ScriptSourceWatcher.reset();
		WHP_EDITOR_WARN(std::string("[Script Watcher] Could not start source watcher: ") + exception.what());
	}
}

void EditorLayer::StopScriptSourceWatcher()
{
	m_ScriptSourceWatcher.reset();
	m_ScriptSourceWatchDirectory.clear();
}

void EditorLayer::HandleScriptSourceEvent(const std::string& path, filewatch::Event eventType)
{
	const std::filesystem::path changedPath(path);
	if (!IsScriptSourceWatchEvent(changedPath, eventType))
		return;

	std::scoped_lock lock(m_ScriptSourceMutex);
	m_ScriptSourceDirty = true;
	m_LastScriptSourceChangeTime = std::chrono::steady_clock::now();
	m_LastScriptSourceChangePath = changedPath;
	m_LastScriptSourceChangeEvent = ScriptSourceEventName(eventType);
}

void EditorLayer::ProcessScriptSourceChanges()
{
	if (!HasProjectLoaded())
		return;

	constexpr auto debounceDuration = std::chrono::milliseconds(750);
	std::filesystem::path changedPath;
	std::string eventName;

	{
		std::scoped_lock lock(m_ScriptSourceMutex);
		if (!m_ScriptSourceDirty)
			return;

		if (std::chrono::steady_clock::now() - m_LastScriptSourceChangeTime < debounceDuration)
			return;

		if (m_SceneState != SceneState::Edit)
		{
			if (!m_ScriptSourceQueuedWhileRunning)
			{
				m_ScriptSourceQueuedWhileRunning = true;
				WHP_EDITOR_INFO("[Script Watcher] Source change detected while scene is running. Build and reload will run after Stop.");
				SetScriptBuildStatus("Script changes queued", true);
			}
			return;
		}

		m_ScriptSourceDirty = false;
		m_ScriptSourceQueuedWhileRunning = false;
		changedPath = m_LastScriptSourceChangePath;
		eventName = m_LastScriptSourceChangeEvent;
	}

	WHP_EDITOR_INFO(std::string("[Script Watcher] Source ") + eventName + ": " + changedPath.generic_string() + ". Building scripts.");
	SetScriptBuildStatus("Script changes detected");
	StopScriptSourceWatcher();
	const bool buildSucceeded = BuildProjectScripts();
	if (buildSucceeded)
	{
		AssemblyManager::ReloadAssembly(true);
		WHP_EDITOR_INFO("[Script Watcher] Scripts rebuilt and reloaded.");
	}
	else
	{
		WHP_EDITOR_WARN("[Script Watcher] Script reload skipped because source build failed.");
	}
	StartScriptSourceWatcher();
}

void EditorLayer::ReloadAssembly(bool resetAppAssemblyFilepath)
{
	if (!HasProjectLoaded())
	{
		WHP_CORE_WARN("[Script Engine] Failed to reload assembly. No Project is loaded.");
		return;
	}

	if (m_SceneState == SceneState::Edit)
	{
		StopScriptSourceWatcher();
		if (!BuildProjectScripts())
		{
			WHP_CORE_WARN("[Script Engine] Assembly reload skipped because script build failed.");
			StartScriptSourceWatcher();
			return;
		}
		AssemblyManager::ReloadAssembly(resetAppAssemblyFilepath);
		StartScriptSourceWatcher();
	}
	else
	{
		SetScriptBuildStatus("Stop scene before reload", true);
		WHP_CORE_WARN("[Script Engine] Failed to reload assembly. Scene is running or simulating!");
	}
}

void EditorLayer::SerializeScene(Ref<Scene> sceneIn, const std::filesystem::path& path)
{
	SceneImporter::SaveScene(sceneIn, path);

	if (!HasProjectLoaded() || !sceneIn)
		return;

	const std::filesystem::path assetDirectory = Project::GetActiveAssetDirectory();
	std::filesystem::path scenePath = path;
	if (!scenePath.is_absolute())
		scenePath = assetDirectory / scenePath;
	scenePath = scenePath.lexically_normal();

	if (!PathIsOrIsUnder(scenePath, assetDirectory))
		return;

	std::error_code error;
	const std::filesystem::path RelativePath = std::filesystem::relative(scenePath, assetDirectory, error).lexically_normal();
	if (error || RelativePath.empty() || !FileExtensions::IsSceneExtension(RelativePath))
		return;

	Ref<EditorAssetManager> EditorAssetManager = Project::GetActive()->GetEditorAssetManager();
	if (!EditorAssetManager)
		return;

	AssetHandle handle = EditorAssetManager->GetHandleFromFilepath(RelativePath);
	if (handle == 0)
		handle = EditorAssetManager->ImportAsset(RelativePath);

	if (handle == 0)
		return;

	sceneIn->m_Handle = handle;
	if (m_EditorScene)
		m_EditorScene->m_Handle = handle;
	if (m_ActiveScene)
		m_ActiveScene->m_Handle = handle;

	EditorAssetManager->SetLoadedAsset(handle, Scene::Copy(sceneIn));
}

void EditorLayer::OnScenePlay()
{
	if (!HasProjectLoaded())
		return;

	if (m_SceneState == SceneState::Simulate)
		OnSceneStop();
	WriteSceneRecoverySnapshot("Before play");
	Project::RunState(true);
	m_SceneState = SceneState::Play;
	ScriptEngine::SetFilewatcherState(false);
	m_ActiveScene = Scene::Copy(m_EditorScene);
	ScriptEngine::SetRuntimeActiveSceneHandle(m_ActiveScene->m_Handle);
	m_ActiveScene->OnRuntimeStart();
	ScriptEngine::SetRuntimeActiveSceneHandle(m_ActiveScene->m_Handle);
	m_LastSelectedEntity = m_SceneHierarchyPanel.GetSelectedEntity();
	m_SceneHierarchyPanel.SetContext(m_ActiveScene);
}

void EditorLayer::OnSceneSimulate()
{
	if (!HasProjectLoaded())
		return;

	if (m_SceneState == SceneState::Play)
		OnSceneStop();

	WriteSceneRecoverySnapshot("Before simulate");
	Project::RunState(true);
	m_SceneState = SceneState::Simulate;
	ScriptEngine::SetFilewatcherState(false);
	m_ActiveScene = Scene::Copy(m_EditorScene);
	ScriptEngine::SetRuntimeActiveSceneHandle(m_ActiveScene->m_Handle);
	m_ActiveScene->OnSimulationStart();
	ScriptEngine::SetRuntimeActiveSceneHandle(m_ActiveScene->m_Handle);
	m_LastSelectedEntity = m_SceneHierarchyPanel.GetSelectedEntity();
	m_SceneHierarchyPanel.SetContext(m_ActiveScene);
	// maybe do not ??
	if(m_LastSelectedEntity)
		m_SceneHierarchyPanel.SetSelectedEntity(m_ActiveScene->FindEntityByUUID(m_LastSelectedEntity.GetUUID()));
}

void EditorLayer::OnSceneStop()
{
	WHP_CORE_ASSERT(m_SceneState == SceneState::Play || m_SceneState == SceneState::Simulate, "invalid SceneState!");
	Project::RunState(false);
	if (m_SceneState == SceneState::Play)
		m_ActiveScene->OnRuntimeStop();
	else if (m_SceneState == SceneState::Simulate)
		m_ActiveScene->OnSimulationStop();
	m_SceneState = SceneState::Edit;
	ScriptEngine::ClearRuntimeSceneTransitionRequest();
	ScriptEngine::SetRuntimeActiveSceneHandle(0);
	ScriptEngine::SetFilewatcherState(true);
	m_ActiveScene = m_EditorScene;
	m_SceneHierarchyPanel.SetContext(m_ActiveScene);
	m_SceneHierarchyPanel.SetSelectedEntity(m_LastSelectedEntity);
}

void EditorLayer::OnScenePause()
{

}

EditorLayer::ProjectHistoryEntry EditorLayer::CaptureProjectHistory() const
{
	ProjectHistoryEntry entry;
	Ref<Project> activeProject = Project::GetActive();
	if (!activeProject || !activeProject->GetEditorAssetManager())
		return entry;

	entry.m_Valid = true;
	entry.m_Config = activeProject->GetConfig();
	entry.m_ProjectPath = activeProject->GetProjectPath();
	entry.m_AssetRegistryPath = activeProject->GetAssetRegistryPath();
	entry.m_ProjectFileContents = ReadTextFile(entry.m_ProjectPath);
	entry.m_AssetRegistryContents = ReadTextFile(entry.m_AssetRegistryPath);

	const AssetRegistry& registry = activeProject->GetEditorAssetManager()->GetAssetRegistry();
	registry.Foreach(AssetType::Scene, [activeProject, &entry](const AssetRegistry::ValueType& value)
		{
			const std::string relativePath = value.second.m_Filepath.generic_string();
			entry.m_SceneFileContents[relativePath] = ReadTextFile(activeProject->GetAssetDirectory() / value.second.m_Filepath);
		});

	return entry;
}

void EditorLayer::RestoreProjectHistory(const ProjectHistoryEntry& entry)
{
	if (!entry.m_Valid)
		return;

	Ref<Project> activeProject = Project::GetActive();
	if (!activeProject || !activeProject->GetEditorAssetManager())
		return;
	if (!entry.m_ProjectPath.empty() && activeProject->GetProjectPath() != entry.m_ProjectPath)
		return;

	std::unordered_set<std::string> currentScenePaths;
	const std::filesystem::path currentAssetDirectory = activeProject->GetAssetDirectory();
	activeProject->GetEditorAssetManager()->GetAssetRegistry().Foreach(AssetType::Scene, [&currentScenePaths](const AssetRegistry::ValueType& value)
		{
			currentScenePaths.insert(value.second.m_Filepath.generic_string());
		});

	activeProject->GetConfig() = entry.m_Config;
	if (!entry.m_ProjectFileContents.empty())
		WriteTextFile(entry.m_ProjectPath, entry.m_ProjectFileContents);
	else
		Project::SaveActive();

	const std::filesystem::path restoredAssetDirectory = entry.m_ProjectPath.parent_path() / entry.m_Config.m_AssetDirectory;
	const std::filesystem::path restoredAssetRegistryPath = restoredAssetDirectory / entry.m_Config.m_AssetRegistryPath;
	if (!entry.m_AssetRegistryContents.empty())
		WriteTextFile(restoredAssetRegistryPath, entry.m_AssetRegistryContents);

	for (const auto& [RelativePath, contents] : entry.m_SceneFileContents)
		WriteTextFile(restoredAssetDirectory / RelativePath, contents);

	for (const std::string& RelativePath : currentScenePaths)
	{
		if (entry.m_SceneFileContents.find(RelativePath) != entry.m_SceneFileContents.end())
			continue;

		std::error_code error;
		std::filesystem::remove(currentAssetDirectory / RelativePath, error);
		if (currentAssetDirectory != restoredAssetDirectory)
			std::filesystem::remove(restoredAssetDirectory / RelativePath, error);
	}

	activeProject->GetEditorAssetManager()->DeserializeAssetRegistry();
	if (m_ContentBrowserPanel)
	{
		m_ContentBrowserPanel = MakeScope<ContentBrowserPanel>(activeProject);
		m_ContentBrowserPanel->SetAssetOpenCallback([this](AssetHandle handle) { return HandleContentBrowserAssetOpen(handle); });
		m_ContentBrowserPanel->SetAssetInspectCallback([this](AssetHandle handle) { return HandleContentBrowserAssetInspect(handle); });
		ApplyPreferencesToContentBrowser();
	}
}

void EditorLayer::CaptureSceneHistory(bool includeProjectSnapshot)
{
	if (m_SceneState != SceneState::Edit || !m_EditorScene)
		return;

	SceneHistoryEntry entry;
	entry.m_SceneSnapshot = Scene::Copy(m_EditorScene);
	entry.m_EditorScenePath = m_EditorScenePath;
	entry.m_SelectedEntities = m_SceneHierarchyPanel.GetSelectedEntityIds();
	if (includeProjectSnapshot)
		entry.m_ProjectSnapshot = CaptureProjectHistory();
	m_UndoStack.push_back(entry);
	m_RedoStack.clear();
	MarkSceneDirty();

	static constexpr size_t maxHistoryEntries = 64;
	if (m_UndoStack.size() > maxHistoryEntries)
		m_UndoStack.erase(m_UndoStack.begin());
}

void EditorLayer::RestoreSceneHistory(const SceneHistoryEntry& entry)
{
	if (!entry.m_SceneSnapshot)
		return;

	if (m_SceneState != SceneState::Edit)
		OnSceneStop();

	RestoreProjectHistory(entry.m_ProjectSnapshot);
	m_EditorScene = Scene::Copy(entry.m_SceneSnapshot);
	m_EditorScenePath = entry.m_EditorScenePath;
	m_EditorScene->OnViewportResize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
	m_ActiveScene = m_EditorScene;
	m_SceneHierarchyPanel.SetContext(m_EditorScene);
	m_SceneHierarchyPanel.SetSelectedEntityIds(entry.m_SelectedEntities);
}

void EditorLayer::UndoScene()
{
	if (m_UndoStack.empty() || m_SceneState != SceneState::Edit)
		return;

	SceneHistoryEntry current;
	current.m_SceneSnapshot = Scene::Copy(m_EditorScene);
	current.m_EditorScenePath = m_EditorScenePath;
	current.m_SelectedEntities = m_SceneHierarchyPanel.GetSelectedEntityIds();
	SceneHistoryEntry entry = m_UndoStack.back();
	if (entry.m_ProjectSnapshot.m_Valid)
		current.m_ProjectSnapshot = CaptureProjectHistory();
	m_RedoStack.push_back(current);

	m_UndoStack.pop_back();
	RestoreSceneHistory(entry);
	MarkSceneDirty();
}

void EditorLayer::RedoScene()
{
	if (m_RedoStack.empty() || m_SceneState != SceneState::Edit)
		return;

	SceneHistoryEntry current;
	current.m_SceneSnapshot = Scene::Copy(m_EditorScene);
	current.m_EditorScenePath = m_EditorScenePath;
	current.m_SelectedEntities = m_SceneHierarchyPanel.GetSelectedEntityIds();
	SceneHistoryEntry entry = m_RedoStack.back();
	if (entry.m_ProjectSnapshot.m_Valid)
		current.m_ProjectSnapshot = CaptureProjectHistory();
	m_UndoStack.push_back(current);

	m_RedoStack.pop_back();
	RestoreSceneHistory(entry);
	MarkSceneDirty();
}

void EditorLayer::ClearSceneHistory()
{
	m_UndoStack.clear();
	m_RedoStack.clear();
	m_GizmoHistoryActive = false;
}

void EditorLayer::OnDuplicatedEntity()
{
	if (m_SceneState != SceneState::Edit)
		return;

	std::vector<Entity> selectedEntities = m_SceneHierarchyPanel.GetSelectedEntities();
	if (selectedEntities.empty())
		return;

	CaptureSceneHistory();
	bool append = false;
	for (Entity selectedEntity : selectedEntities)
	{
		Entity duplicated = m_EditorScene->DuplicateEntity(selectedEntity);
		m_SceneHierarchyPanel.SetSelectedEntity(duplicated, append);
		append = true;
	}
}

void EditorLayer::OnDeletedEntity()
{
	if(Application::Get().GetImGuiLayer()->GetActiveWidgetID() == 0)
	{
		std::vector<Entity> selectedEntities = m_SceneHierarchyPanel.GetSelectedEntities();
		if (!selectedEntities.empty())
		{
			CaptureSceneHistory();
			std::vector<UUID> selectedIds;
			selectedIds.reserve(selectedEntities.size());
			for (Entity selectedEntity : selectedEntities)
				selectedIds.push_back(selectedEntity.GetUUID());

			auto hasSelectedAncestor = [&](Entity selectedEntity)
				{
					while (selectedEntity && selectedEntity.HasComponent<HierarchyComponent>())
					{
						UUID parentId = selectedEntity.GetComponent<HierarchyComponent>().m_Parent;
						if (parentId == 0)
							return false;
						if (std::find(selectedIds.begin(), selectedIds.end(), parentId) != selectedIds.end())
							return true;
						selectedEntity = m_ActiveScene->FindEntityByUUID(parentId);
					}
					return false;
				};

			m_SceneHierarchyPanel.ClearSelection();
			for (Entity selectedEntity : selectedEntities)
				if (selectedEntity && !hasSelectedAncestor(selectedEntity))
					m_ActiveScene->DestroyEntity(selectedEntity);
		}
	}
}

void EditorLayer::OnSelectAllEntities()
{
	if (m_SceneState == SceneState::Edit)
		m_SceneHierarchyPanel.SelectAll();
}

void EditorLayer::OnCopyEntities()
{
	m_EntityClipboard = m_SceneHierarchyPanel.GetSelectedEntityIds();
}

void EditorLayer::OnPasteEntities()
{
	if (m_SceneState != SceneState::Edit || m_EntityClipboard.empty())
		return;

	std::vector<Entity> sourceEntities;
	for (UUID id : m_EntityClipboard)
	{
		Entity source = m_EditorScene->FindEntityByUUID(id);
		if (source)
			sourceEntities.push_back(source);
	}

	if (sourceEntities.empty())
		return;

	CaptureSceneHistory();
	bool append = false;
	for (Entity source : sourceEntities)
	{
		Entity pasted = m_EditorScene->DuplicateEntity(source);
		m_SceneHierarchyPanel.SetSelectedEntity(pasted, append);
		append = true;
	}
}

void EditorLayer::OnCutEntities()
{
	OnCopyEntities();
	OnDeletedEntity();
}

void EditorLayer::UIToolbar()
{
	bool toolbarEnabled = (bool)m_SceneHierarchyPanel.GetContext();

	ImVec4 tintColor = ImVec4(1, 1, 1, 1);
	if (!toolbarEnabled)
		tintColor.w = 0.5f;

	bool hasPlayButton = m_SceneState == SceneState::Edit|| m_SceneState == SceneState::Play;
	bool hasSimulateButton = m_SceneState == SceneState::Edit || m_SceneState == SceneState::Simulate;
	bool hasPauseButton = m_SceneState != SceneState::Edit;
	bool isPaused = hasPauseButton && m_ActiveScene->IsPaused();
	bool hasStepButton = hasPauseButton && isPaused;

	const float buttonSize = 36.0f;
	const float iconSize = 18.0f;
	const float padding = 6.0f;
	const float spacing = 5.0f;
	const int buttonCount = (hasPlayButton ? 1 : 0) + (hasSimulateButton ? 1 : 0) + (hasPauseButton ? 1 : 0) + (hasStepButton ? 1 : 0);
	const float panelWidth = padding * 2.0f + buttonSize * buttonCount + spacing * glm::max(buttonCount - 1, 0);
	const float panelHeight = buttonSize + padding * 2.0f;

	ImVec2 viewportMin = ImVec2(m_ViewportBounds[0].x, m_ViewportBounds[0].y);
	ImVec2 viewportMax = ImVec2(m_ViewportBounds[1].x, m_ViewportBounds[1].y);
	ImVec2 panelPos = ImVec2(viewportMin.x + ((viewportMax.x - viewportMin.x) - panelWidth) * 0.5f, viewportMin.y + 12.0f);
	ImVec2 panelEnd = ImVec2(panelPos.x + panelWidth, panelPos.y + panelHeight);

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	drawList->AddRectFilled(ImVec2(panelPos.x + 2.0f, panelPos.y + 3.0f), ImVec2(panelEnd.x + 2.0f, panelEnd.y + 3.0f), IM_COL32(0, 0, 0, 76), 7.0f);
	drawList->AddRectFilled(panelPos, panelEnd, IM_COL32(24, 22, 19, 238), 7.0f);
	drawList->AddRect(panelPos, panelEnd, IM_COL32(76, 64, 48, 210), 7.0f);

	ImGui::SetCursorScreenPos(ImVec2(panelPos.x + padding, panelPos.y + padding));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(spacing, 0.0f));

	auto drawIconButton = [&](const char* id, Icon iconType, ImU32 accent, const char* tooltip) -> bool
		{
			Ref<Texture2D> iconTexture = IconManager::Get().GetIcon(iconType);
			ImGui::InvisibleButton(id, ImVec2(buttonSize, buttonSize));
			const bool clicked = ImGui::IsItemClicked() && toolbarEnabled;
			const bool hovered = ImGui::IsItemHovered();
			const bool active = ImGui::IsItemActive();
			ImVec2 min = ImGui::GetItemRectMin();
			ImVec2 max = ImGui::GetItemRectMax();
			ImU32 buttonColor = active ? ColorU32(0.33f, 0.22f, 0.12f, 0.95f) : hovered ? ColorU32(0.18f, 0.15f, 0.12f, 0.92f) : ColorU32(0.10f, 0.09f, 0.08f, 0.88f);
			drawList->AddRectFilled(min, max, buttonColor, 5.0f);
			if (hovered)
				drawList->AddRect(min, max, accent, 5.0f);

			ImVec2 center((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
			ImVec2 iconMin(center.x - iconSize * 0.5f, center.y - iconSize * 0.5f);
			ImVec2 iconMax(center.x + iconSize * 0.5f, center.y + iconSize * 0.5f);
			ImU32 tint = toolbarEnabled ? IM_COL32(240, 232, 216, 255) : IM_COL32(148, 140, 128, 190);
			drawList->AddImage(UI::ToImGuiTextureId(iconTexture->GetRendererId()), iconMin, iconMax, ImVec2(0, 1), ImVec2(1, 0), tint);
			if (hovered && tooltip)
				ImGui::SetTooltip("%s", tooltip);
			return clicked;
		};

	if(hasPlayButton)
	{
		Icon playIcon = m_SceneState == SceneState::Play ? Icon::Stop : Icon::Play;
		if (drawIconButton("##ViewportToolbarPlay", playIcon, ColorU32(0.58f, 0.70f, 0.42f, tintColor.w), m_SceneState == SceneState::Play ? "Stop" : "Play"))
		{
			if (m_SceneState == SceneState::Edit || m_SceneState == SceneState::Simulate)
				OnScenePlay();
			else if (m_SceneState == SceneState::Play)
				OnSceneStop();
		}
	}
	if(hasSimulateButton)
	{
		if(hasPlayButton)
			ImGui::SameLine();
		Icon simulateIcon = m_SceneState == SceneState::Simulate ? Icon::Stop : Icon::Simulate;
		if (drawIconButton("##ViewportToolbarSimulate", simulateIcon, ColorU32(0.66f, 0.55f, 0.42f, tintColor.w), m_SceneState == SceneState::Simulate ? "Stop simulation" : "Simulate"))
		{
			if (m_SceneState == SceneState::Edit || m_SceneState == SceneState::Play)
				OnSceneSimulate();
			else if (m_SceneState == SceneState::Simulate)
				OnSceneStop();
		}
	}
	if (hasPauseButton)
	{
		ImGui::SameLine();
		if (drawIconButton("##ViewportToolbarPause", Icon::Pause, ColorU32(0.86f, 0.64f, 0.32f, tintColor.w), isPaused ? "Resume" : "Pause"))
			m_ActiveScene->SetPaused(!isPaused);

		if (isPaused)
		{
			ImGui::SameLine();
			if (drawIconButton("##ViewportToolbarStepForward", Icon::StepForward, ColorU32(0.86f, 0.64f, 0.32f, tintColor.w), "Step"))
				m_ActiveScene->Step(m_UISettings.GetStepFrame());
		}
	}
	ImGui::PopStyleVar();

	if (HasProjectLoaded() && !m_ScriptBuildStatus.empty())
	{
		const ImVec2 textSize = ImGui::CalcTextSize(m_ScriptBuildStatus.c_str());
		const float statusPaddingX = 10.0f;
		const float statusHeight = 24.0f;
		const float statusWidth = glm::min(textSize.x + statusPaddingX * 2.0f, 260.0f);
		ImVec2 statusPos(panelEnd.x + 10.0f, panelPos.y + (panelHeight - statusHeight) * 0.5f);
		if (statusPos.x + statusWidth > viewportMax.x - 10.0f)
			statusPos = ImVec2(panelPos.x - statusWidth - 10.0f, statusPos.y);

		if (statusPos.x > viewportMin.x + 10.0f)
		{
			const ImU32 statusFill = m_ScriptBuildStatusFailure ? IM_COL32(84, 34, 32, 230) :
				m_ScriptBuildStatusWarning ? IM_COL32(78, 58, 28, 230) : IM_COL32(34, 62, 48, 220);
			const ImU32 statusBorder = m_ScriptBuildStatusFailure ? IM_COL32(214, 94, 84, 230) :
				m_ScriptBuildStatusWarning ? IM_COL32(226, 174, 74, 230) : IM_COL32(112, 184, 136, 220);
			ImVec2 statusEnd(statusPos.x + statusWidth, statusPos.y + statusHeight);
			drawList->AddRectFilled(statusPos, statusEnd, statusFill, 5.0f);
			drawList->AddRect(statusPos, statusEnd, statusBorder, 5.0f);
			drawList->AddText(ImVec2(statusPos.x + statusPaddingX, statusPos.y + 4.0f), IM_COL32(238, 232, 220, 255), m_ScriptBuildStatus.c_str());
			if (ImGui::IsMouseHoveringRect(statusPos, statusEnd))
				ImGui::SetTooltip("%s", m_ScriptBuildStatus.c_str());
		}
	}
}

_WHIP_END
