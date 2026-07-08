#include <WhipPch.h>

#include <Whip-Editor/Helpers/EditorProcessUtils.h>

#include <array>
#include <cstdio>
#include <format>
#include <sstream>
#include <vector>

#ifdef _WIN32
	#ifndef NOMINMAX
		#define NOMINMAX
	#endif
	#include <Windows.h>
#endif

_WHIP_START

namespace EditorProcess
{
	namespace
	{
		void EmitLine(const RunOptions& options, RunResult& result, std::string line)
		{
			if (!line.empty() && line.back() == '\r')
				line.pop_back();
			if (line.empty())
				return;

			if (result.m_FirstLine.empty())
				result.m_FirstLine = line;
			if (options.m_OnOutput)
				options.m_OnOutput(line);
			if (options.m_LogOutput)
				WHP_EDITOR_INFO("{0}{1}", options.m_LogPrefix, line);
		}

		void FlushPending(const RunOptions& options, RunResult& result, std::string& pending)
		{
			size_t newline = std::string::npos;
			while ((newline = pending.find('\n')) != std::string::npos)
			{
				std::string line = pending.substr(0, newline);
				pending.erase(0, newline + 1);
				EmitLine(options, result, std::move(line));
			}
		}
	}

	std::string QuoteCommandPath(const std::filesystem::path& path)
	{
		return std::format("\"{}\"", path.string());
	}

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

	RunResult RunCommand(const std::string& command, const RunOptions& options)
	{
		RunResult result;
		if (command.empty())
			return result;

#ifdef _WIN32
		SECURITY_ATTRIBUTES securityAttributes{};
		securityAttributes.nLength = sizeof(SECURITY_ATTRIBUTES);
		securityAttributes.bInheritHandle = TRUE;

		HANDLE readPipe = nullptr;
		HANDLE writePipe = nullptr;
		if (!CreatePipe(&readPipe, &writePipe, &securityAttributes, 0))
		{
			WHP_EDITOR_ERROR("{0}Could not create process output pipe.", options.m_LogPrefix);
			return result;
		}
		SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

		STARTUPINFOA startupInfo{};
		startupInfo.cb = sizeof(STARTUPINFOA);
		startupInfo.dwFlags = STARTF_USESTDHANDLES;
		if (options.m_HideWindow)
		{
			startupInfo.dwFlags |= STARTF_USESHOWWINDOW;
			startupInfo.wShowWindow = SW_HIDE;
		}
		startupInfo.hStdOutput = writePipe;
		startupInfo.hStdError = writePipe;
		startupInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

		PROCESS_INFORMATION processInfo{};
		std::string mutableCommand = command;
		const std::string workingDirectory = options.m_WorkingDirectory.empty() ? std::string{} : options.m_WorkingDirectory.string();
		BOOL created = CreateProcessA(
			nullptr,
			mutableCommand.data(),
			nullptr,
			nullptr,
			TRUE,
			options.m_HideWindow ? CREATE_NO_WINDOW : 0,
			nullptr,
			workingDirectory.empty() ? nullptr : workingDirectory.c_str(),
			&startupInfo,
			&processInfo);
		CloseHandle(writePipe);

		if (!created)
		{
			const DWORD error = GetLastError();
			CloseHandle(readPipe);
			WHP_EDITOR_ERROR("{0}Could not start process. Windows error {1}. Command: {2}", options.m_LogPrefix, error, command);
			return result;
		}

		result.m_Started = true;

		std::string pending;
		std::array<char, 2048> buffer{};
		bool processRunning = true;
		while (processRunning)
		{
			DWORD available = 0;
			while (PeekNamedPipe(readPipe, nullptr, 0, nullptr, &available, nullptr) && available > 0)
			{
				DWORD bytesRead = 0;
				const DWORD bytesToRead = std::min<DWORD>(available, static_cast<DWORD>(buffer.size() - 1));
				if (!ReadFile(readPipe, buffer.data(), bytesToRead, &bytesRead, nullptr) || bytesRead == 0)
					break;
				buffer[bytesRead] = '\0';
				pending.append(buffer.data(), bytesRead);
				FlushPending(options, result, pending);
				available = 0;
			}

			if (options.m_IsCancellationRequested && options.m_IsCancellationRequested())
			{
				result.m_Cancelled = true;
				TerminateProcess(processInfo.hProcess, ERROR_CANCELLED);
			}

			const DWORD waitResult = WaitForSingleObject(processInfo.hProcess, 50);
			processRunning = waitResult == WAIT_TIMEOUT;
		}

		DWORD remaining = 0;
		while (PeekNamedPipe(readPipe, nullptr, 0, nullptr, &remaining, nullptr) && remaining > 0)
		{
			DWORD bytesRead = 0;
			const DWORD bytesToRead = std::min<DWORD>(remaining, static_cast<DWORD>(buffer.size() - 1));
			if (!ReadFile(readPipe, buffer.data(), bytesToRead, &bytesRead, nullptr) || bytesRead == 0)
				break;
			buffer[bytesRead] = '\0';
			pending.append(buffer.data(), bytesRead);
			FlushPending(options, result, pending);
			remaining = 0;
		}

		if (!pending.empty())
			EmitLine(options, result, std::move(pending));

		DWORD exitCode = 0;
		GetExitCodeProcess(processInfo.hProcess, &exitCode);
		result.m_ExitCode = static_cast<int>(exitCode);

		CloseHandle(processInfo.hProcess);
		CloseHandle(processInfo.hThread);
		CloseHandle(readPipe);
		return result;
#else
		FILE* pipe = popen(command.c_str(), "r");
		if (!pipe)
			return result;

		result.m_Started = true;
		std::array<char, 2048> buffer{};
		while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe))
		{
			std::string line(buffer.data());
			while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
				line.pop_back();
			EmitLine(options, result, std::move(line));
		}

		result.m_ExitCode = pclose(pipe);
		return result;
#endif
	}
}

_WHIP_END
