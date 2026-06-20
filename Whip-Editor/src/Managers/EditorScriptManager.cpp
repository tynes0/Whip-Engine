#include <Whip-Editor/Managers/EditorScriptManager.h>

#include <Whip/Scripting/ScriptEngine.h>
#include <Whip/Scripting/ScriptProjectGenerator.h>

#include <algorithm>
#include <array>
#include <format>
#include <fstream>
#include <iterator>
#include <sstream>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

_WHIP_START

namespace
{
	std::string LowerCopy(std::string value)
	{
		std::ranges::transform(value, value.begin(),
		                       [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
		return value;
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
		std::string fallback = SanitizePathToken(config.m_Name, "Untitled");
		if (!config.m_ScriptModulePath.empty())
		{
			const std::string moduleName = config.m_ScriptModulePath.stem().string();
			if (!moduleName.empty())
				return SanitizePathToken(moduleName, fallback);
		}

		return fallback;
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

EditorScriptManager::EditorScriptManager(EditorLayer* boundedLayer)
	: EditorManagerBase(boundedLayer)
{
}

EditorScriptManager::~EditorScriptManager()
{
	StopSourceWatcher();
}

bool EditorScriptManager::WriteProjectFiles(const std::filesystem::path& projectDirectory, const std::string& projectFolderName)
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

bool EditorScriptManager::BuildProjectScripts()
{
	if (!Project::GetActive())
		return false;

	const ProjectConfig& config = Project::GetActive()->GetConfig();
	if (config.m_ScriptModulePath.empty())
	{
		WHP_EDITOR_INFO("[Script Build] Project has no script module configured.");
		SetStatus("No script module", true);
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
		if (const bool generatedSdkProject = projectFileContents.find("<Project Sdk=\"Microsoft.NET.Sdk\">") != std::string::npos; generatedSdkProject)
		{
			const std::string solutionFileContents = ReadTextFile(preferredSolutionFile);
			const std::string buildPropsContents = buildPropsExists ? ReadTextFile(buildPropsFile) : "";
			needsWorkspaceRefresh =
				!buildPropsExists ||
				projectFileContents.find(R"(<ProjectReference Include="Whip-ScriptCore\Whip-ScriptCore.csproj">)") == std::string::npos ||
				projectFileContents.find(R"(<Compile Remove="Whip-ScriptCore\**\*.cs" />)") == std::string::npos ||
				projectFileContents.find(R"(<Compile Remove="Intermediates\**\*.cs" />)") == std::string::npos ||
				projectFileContents.find(R"(<Compile Remove="obj\**\*.cs" />)") == std::string::npos ||
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
		SetStatus("No C# Project found", true);
		return true;
	}

	const ScriptBuildCommand buildCommand = MakeScriptBuildCommand(scriptProjectFile);
	if (buildCommand.command.empty())
	{
		WHP_EDITOR_WARN("[Script Build] Could not find MSBuild.exe or dotnet. Set WHIP_MSBUILD_PATH or add MSBuild/dotnet to PATH. Reloading the existing assembly if it exists.");
		SetStatus("MSBuild/dotnet not found", true, true);
		return true;
	}

	WHP_EDITOR_INFO(std::string("[Script Build] Building ") + scriptProjectFile.string() + " with " + buildCommand.toolName);
	SetStatus("Building scripts...");
	const int result = RunCommandAndLogOutput(buildCommand.command);
	if (result != 0)
	{
		WHP_EDITOR_ERROR(std::string("[Script Build] Build failed with exit code ") + std::to_string(result) + ".");
		SetStatus("Script build failed", false, true);
		return false;
	}

	WHP_EDITOR_INFO("[Script Build] Build succeeded.");
	SetStatus("Scripts built");
	return true;
}

void EditorScriptManager::ReloadAssembly(bool resetAppAssemblyFilepath, bool sceneEditable)
{
	if (!Project::GetActive())
	{
		WHP_CORE_WARN("[Script Engine] Failed to reload assembly. No Project is loaded.");
		return;
	}

	if (sceneEditable)
	{
		StopSourceWatcher();
		if (!BuildProjectScripts())
		{
			WHP_CORE_WARN("[Script Engine] Assembly reload skipped because script build failed.");
			StartSourceWatcher();
			return;
		}
		AssemblyManager::ReloadAssembly(resetAppAssemblyFilepath);
		StartSourceWatcher();
	}
	else
	{
		SetStatus("Stop scene before reload", true);
		WHP_CORE_WARN("[Script Engine] Failed to reload assembly. Scene is running or simulating!");
	}
}

void EditorScriptManager::StartSourceWatcher()
{
	StopSourceWatcher();

	if (!Project::GetActive())
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
		m_SourceWatchDirectory = scriptsDirectory;
		m_SourceWatcher = MakeScope<filewatch::FileWatch<std::string>>(
			scriptsDirectory.string(),
			[this](const std::string& path, const filewatch::Event eventType)
			{
				HandleSourceEvent(path, eventType);
			});
		WHP_EDITOR_INFO(std::string("[Script Watcher] Watching C# source changes under ") + scriptsDirectory.string());
	}
	catch (const std::exception& exception)
	{
		m_SourceWatcher.reset();
		WHP_EDITOR_WARN(std::string("[Script Watcher] Could not start source watcher: ") + exception.what());
	}
}

void EditorScriptManager::StopSourceWatcher()
{
	m_SourceWatcher.reset();
	m_SourceWatchDirectory.clear();
}

void EditorScriptManager::HandleSourceEvent(const std::string& path, filewatch::Event eventType)
{
	const std::filesystem::path changedPath(path);
	if (!IsScriptSourceWatchEvent(changedPath, eventType))
		return;

	std::scoped_lock lock(m_SourceMutex);
	m_SourceDirty = true;
	m_LastSourceChangeTime = std::chrono::steady_clock::now();
	m_LastSourceChangePath = changedPath;
	m_LastSourceChangeEvent = ScriptSourceEventName(eventType);
}

void EditorScriptManager::ProcessSourceChanges(bool sceneEditable)
{
	if (!Project::GetActive())
		return;

	constexpr auto debounceDuration = std::chrono::milliseconds(750);
	std::filesystem::path changedPath;
	std::string eventName;

	{
		std::scoped_lock lock(m_SourceMutex);
		if (!m_SourceDirty)
			return;

		if (std::chrono::steady_clock::now() - m_LastSourceChangeTime < debounceDuration)
			return;

		if (!sceneEditable)
		{
			if (!m_SourceQueuedWhileRunning)
			{
				m_SourceQueuedWhileRunning = true;
				WHP_EDITOR_INFO("[Script Watcher] Source change detected while scene is running. Build and reload will run after Stop.");
				SetStatus("Script changes queued", true);
			}
			return;
		}

		m_SourceDirty = false;
		m_SourceQueuedWhileRunning = false;
		changedPath = m_LastSourceChangePath;
		eventName = m_LastSourceChangeEvent;
	}

	WHP_EDITOR_INFO(std::string("[Script Watcher] Source ") + eventName + ": " + changedPath.generic_string() + ". Building scripts.");
	SetStatus("Script changes detected");
	StopSourceWatcher();
	if (const bool buildSucceeded = BuildProjectScripts(); buildSucceeded)
	{
		AssemblyManager::ReloadAssembly(true);
		WHP_EDITOR_INFO("[Script Watcher] Scripts rebuilt and reloaded.");
	}
	else
	{
		WHP_EDITOR_WARN("[Script Watcher] Script reload skipped because source build failed.");
	}
	StartSourceWatcher();
}

void EditorScriptManager::Reset()
{
	StopSourceWatcher();
	{
		std::scoped_lock lock(m_SourceMutex);
		m_SourceDirty = false;
		m_SourceQueuedWhileRunning = false;
		m_LastSourceChangePath.clear();
		m_LastSourceChangeEvent.clear();
	}
	SetStatus("Scripts idle");
}

void EditorScriptManager::SetStatus(const std::string& message, bool warning, bool failure)
{
	m_Status.m_Message = message;
	m_Status.m_Time = std::chrono::steady_clock::now();
	m_Status.m_Warning = warning;
	m_Status.m_Failure = failure;
}

const EditorScriptManager::Status& EditorScriptManager::GetStatus() const
{
	return m_Status;
}

_WHIP_END
