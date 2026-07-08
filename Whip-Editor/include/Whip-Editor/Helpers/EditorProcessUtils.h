#pragma once

#include <Whip/Core/Core.h>

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>

_WHIP_START

namespace EditorProcess
{
	using OutputCallback = std::function<void(std::string_view)>;
	using CancelCallback = std::function<bool()>;

	struct RunOptions
	{
		std::filesystem::path m_WorkingDirectory;
		OutputCallback m_OnOutput;
		CancelCallback m_IsCancellationRequested;
		std::string m_LogPrefix;
		bool m_LogOutput = true;
		bool m_HideWindow = true;
	};

	struct RunResult
	{
		int m_ExitCode = -1;
		bool m_Started = false;
		bool m_Cancelled = false;
		std::string m_FirstLine;
	};

	std::string QuoteCommandPath(const std::filesystem::path& path);
	std::filesystem::path PathFromEnvironment(const char* name);
	std::filesystem::path FindExecutableInPath(const std::string& executableName);
	RunResult RunCommand(const std::string& command, const RunOptions& options = {});
}

_WHIP_END
