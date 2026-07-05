#pragma once

#include <filesystem>
#include <memory>
#include <string>

#include <Whip/Asset/EditorAssetManager.h>
#include <Whip/Asset/RuntimeAssetManager.h>
#include <Whip/Core/Core.h>
#include <Whip/Core/Log.h>
#include <Whip/Core/Memory.h>
#include <Whip/Utils/FileExtensions.h>

_WHIP_START

struct ProjectConfig
{
	std::string m_Name = "Untitled";

	AssetHandle m_StartScene = 0;

	std::filesystem::path m_AssetDirectory;
	std::filesystem::path m_CacheDirectory;
	std::filesystem::path m_AssetRegistryPath = FileExtensions::AssetRegistryFilename;
	std::filesystem::path m_ScriptModulePath;

#if defined(WHP_DEBUG)
	bool m_EnableScriptDebugging = true;
#else
	bool m_EnableScriptDebugging = false;
#endif
	std::string m_ScriptDebuggerHost = "127.0.0.1";
	int m_ScriptDebuggerPort = 2550;
	bool m_ScriptDebuggerSuspendOnStart = false;
	std::string m_ScriptDebuggerLogFile = "MonoDebugger.log";
};

class Project
{
public:
	const std::filesystem::path& GetProjectDirectory()
	{
		return m_ProjectDirectory;
	}

	const std::filesystem::path& GetProjectPath()
	{
		return m_ProjectPath;
	}

	void SetProjectPath(const std::filesystem::path& path)
	{
		m_ProjectPath = path;
		m_ProjectDirectory = path.parent_path();
	}

	std::filesystem::path GetAssetDirectory()
	{
		return GetProjectDirectory() / m_Config.m_AssetDirectory;
	}

	std::filesystem::path GetAssetRegistryPath()
	{
		return GetAssetDirectory() / m_Config.m_AssetRegistryPath;
	}

	std::filesystem::path GetAssetFileSystemPath(const std::filesystem::path& path)
	{
		return GetAssetDirectory() / path;
	}

	std::filesystem::path GetAssetAbsolutePath(const std::filesystem::path& path);

	static const std::filesystem::path& GetActiveProjectDirectory()
	{
		WHP_CORE_ASSERT(s_ActiveProject);
		return s_ActiveProject->GetProjectDirectory();
	}

	static const std::filesystem::path& GetActiveProjectPath()
	{
		WHP_CORE_ASSERT(s_ActiveProject);
		return s_ActiveProject->GetProjectPath();
	}

	static void SetActiveProjectPath(const std::filesystem::path& path)
	{
		WHP_CORE_ASSERT(s_ActiveProject);
		s_ActiveProject->SetProjectPath(path);
	}

	static std::filesystem::path GetActiveAssetDirectory()
	{
		WHP_CORE_ASSERT(s_ActiveProject);
		return s_ActiveProject->GetAssetDirectory();
	}

	static std::filesystem::path GetActiveAssetRegistryPath()
	{
		WHP_CORE_ASSERT(s_ActiveProject);
		return s_ActiveProject->GetAssetRegistryPath();
	}

	static std::filesystem::path GetActiveAssetFileSystemPath(const std::filesystem::path& path)
	{
		WHP_CORE_ASSERT(s_ActiveProject);
		return s_ActiveProject->GetAssetFileSystemPath(path);
	}

	ProjectConfig& GetConfig() { return m_Config; }
	const ProjectConfig& GetConfig() const { return m_Config; }

	static Ref<Project> GetActive() { return s_ActiveProject; }
	static void SetActive(const Ref<Project>& project) { s_ActiveProject = project; }

	std::shared_ptr<AssetManagerBase> GetAssetManager() { return m_Running ? m_RuntimeAssetManager : m_EditorAssetManager; }
	std::shared_ptr<RuntimeAssetManager> GetRuntimeAssetManager() const { return std::static_pointer_cast<RuntimeAssetManager>(m_RuntimeAssetManager); }
	std::shared_ptr<EditorAssetManager> GetEditorAssetManager() const { return std::static_pointer_cast<EditorAssetManager>(m_EditorAssetManager); }

	static Ref<Project> NewProject();
	static Ref<Project> LoadDetached(const std::filesystem::path& path);
	static Ref<Project> Load(const std::filesystem::path& path);
	static bool SaveActive();
	static bool SaveActive(const std::filesystem::path& path);

	static void RunState(bool running) { s_ActiveProject->m_Running = running; }
	static bool Running() { return s_ActiveProject->m_Running; }
	static bool Loaded() { return s_ActiveProject->m_Loaded; }
private:
	bool m_Loaded = false;
	bool m_Running = false;
	ProjectConfig m_Config;
	std::filesystem::path m_ProjectDirectory;
	std::filesystem::path m_ProjectPath;
	std::shared_ptr<AssetManagerBase> m_EditorAssetManager;
	std::shared_ptr<AssetManagerBase> m_RuntimeAssetManager;

	inline static Ref<Project> s_ActiveProject;
};

_WHIP_END
