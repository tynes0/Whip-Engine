#pragma once

#include <string>
#include <filesystem>

#include <Whip/Core/Core.h>
#include <Whip/Core/Log.h>
#include <Whip/Core/memory.h>
#include <Whip/Asset/editor_asset_manager.h>
#include <Whip/Asset/runtime_asset_manager.h>

_WHIP_START

struct project_config
{
	std::string name = "Untitled";

	asset_handle start_scene;

	std::filesystem::path asset_directory;
	std::filesystem::path cache_directory; // Unused for now
	std::filesystem::path asset_registry_path; // relative to asset directory
	std::filesystem::path script_module_path;
};

class project
{
public:
	const std::filesystem::path& get_project_directory()
	{
		return s_active_project->m_project_directory;
	}

	const std::filesystem::path& get_project_path()
	{
		return s_active_project->m_project_path;
	}

	void set_project_path(const std::filesystem::path& path)
	{
		s_active_project->m_project_path = path;
		s_active_project->m_project_directory = path.parent_path();
	}

	std::filesystem::path get_asset_directory()
	{
		return get_project_directory() / s_active_project->m_config.asset_directory;
	}

	std::filesystem::path get_asset_registry_path()
	{
		return get_asset_directory() / s_active_project->m_config.asset_registry_path;
	}

	// TODO: move to asset manager when we have one
	std::filesystem::path get_asset_file_system_path(const std::filesystem::path& path)
	{
		return get_asset_directory() / path;
	}

	std::filesystem::path get_asset_absolute_path(const std::filesystem::path& path);

	static const std::filesystem::path& get_active_project_directory()
	{
		WHP_CORE_ASSERT(s_active_project);
		return s_active_project->get_project_directory();
	}

	static const std::filesystem::path& get_active_project_path()
	{
		WHP_CORE_ASSERT(s_active_project);
		return s_active_project->get_project_path();
	}

	static void set_active_project_path(const std::filesystem::path& path)
	{
		WHP_CORE_ASSERT(s_active_project);
		s_active_project->set_project_path(path);
	}

	static std::filesystem::path get_active_asset_directory()
	{
		WHP_CORE_ASSERT(s_active_project);
		return s_active_project->get_asset_directory();
	}

	static std::filesystem::path get_active_asset_registry_path() 
	{
		WHP_CORE_ASSERT(s_active_project);
		return s_active_project->get_asset_registry_path();
	}

	// TODO: move to asset manager when we have one
	static std::filesystem::path get_active_asset_file_system_path(const std::filesystem::path& path)
	{
		WHP_CORE_ASSERT(s_active_project);
		return s_active_project->get_asset_file_system_path(path);
	}

	project_config& get_config() { return m_config; }

	static ref<project> get_active() { return s_active_project; }
	static void set_active(const ref<project>& proj) { s_active_project = proj; }
		   
	std::shared_ptr<asset_manager_base> get_asset_manager() { return m_running ? m_runtime_asset_manager : m_editor_asset_manager; }
	std::shared_ptr<runtime_asset_manager> get_runtime_asset_manager() const { return std::static_pointer_cast<runtime_asset_manager>(m_runtime_asset_manager); }
	std::shared_ptr<editor_asset_manager> get_editor_asset_manager() const { return std::static_pointer_cast<editor_asset_manager>(m_editor_asset_manager); }

	static ref<project> new_project();
	static ref<project> load(const std::filesystem::path& path);
	static bool save_active();
	static bool save_active(const std::filesystem::path& path);

	static void run_state(bool running) { s_active_project->m_running = running; }
	static bool running() { return s_active_project->m_running; }
	static bool loaded() { return s_active_project->m_loaded; }
private:
	bool m_loaded = false;
	bool m_running = false;
	project_config m_config;
	std::filesystem::path m_project_directory;
	std::filesystem::path m_project_path;

	std::shared_ptr<asset_manager_base> m_editor_asset_manager;
	std::shared_ptr<asset_manager_base> m_runtime_asset_manager;

	inline static ref<project> s_active_project;
};

_WHIP_END
