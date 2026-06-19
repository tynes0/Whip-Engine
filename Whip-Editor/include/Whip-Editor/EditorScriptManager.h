#pragma once

#include <Whip.h>

#include <FileWatch.h>

#include <chrono>
#include <filesystem>
#include <mutex>
#include <string>

_WHIP_START

class EditorScriptManager
{
public:
	struct Status
	{
		std::string m_Message = "Scripts idle";
		std::chrono::steady_clock::time_point m_Time{};
		bool m_Warning = false;
		bool m_Failure = false;
	};

	~EditorScriptManager();

	bool BuildProjectScripts();
	void ReloadAssembly(bool resetAppAssemblyFilepath, bool sceneEditable);
	void StartSourceWatcher();
	void StopSourceWatcher();
	void ProcessSourceChanges(bool sceneEditable);
	void Reset();

	void SetStatus(const std::string& message, bool warning = false, bool failure = false);
	const Status& GetStatus() const { return m_Status; }

	static bool WriteProjectFiles(const std::filesystem::path& projectDirectory, const std::string& projectFolderName);

private:
	void HandleSourceEvent(const std::string& path, filewatch::Event eventType);

	Scope<filewatch::FileWatch<std::string>> m_SourceWatcher;
	std::filesystem::path m_SourceWatchDirectory;
	std::mutex m_SourceMutex;
	std::chrono::steady_clock::time_point m_LastSourceChangeTime{};
	std::filesystem::path m_LastSourceChangePath;
	std::string m_LastSourceChangeEvent;
	bool m_SourceDirty = false;
	bool m_SourceQueuedWhileRunning = false;
	Status m_Status;
};

_WHIP_END
