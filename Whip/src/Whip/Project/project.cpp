#include "whippch.h"
#include "project.h"
#include "project_serializer.h"
#include <Whip/Utils/platform_utils.h>

_WHIP_START

std::filesystem::path project::get_asset_absolute_path(const std::filesystem::path& path)
{
	return get_asset_directory() / path;
}

ref<project> project::new_project()
{
	s_active_project = make_ref<project>();
	s_active_project->m_loaded = true;
	return s_active_project;
}

ref<project> project::load(const std::filesystem::path& path)
{
	ref<project> result = make_ref<project>();
	project_serializer serializer(result);
	if (serializer.deserialize(path))
	{
		result->m_project_path = path;
		result->m_project_directory = path.parent_path();
		s_active_project = result;
		std::shared_ptr<editor_asset_manager> editor_asset_mnger = std::make_shared<editor_asset_manager>();
		editor_asset_mnger->deserialize_asset_registry();
		s_active_project->m_editor_asset_manager = editor_asset_mnger;
		std::shared_ptr<runtime_asset_manager> runtime_asset_mnger = std::make_shared<runtime_asset_manager>();
		runtime_asset_mnger->set_editor_asset_manager(editor_asset_mnger);
		s_active_project->m_runtime_asset_manager = runtime_asset_mnger;
		s_active_project->m_loaded = true;
		return s_active_project;
	}
	return nullptr;
}

bool project::save_active()
{
	project_serializer serializer(s_active_project);
	if (serializer.serialize(s_active_project->m_project_path))
		return true;
	return false;
}

bool project::save_active(const std::filesystem::path& path)
{
	project_serializer serializer(s_active_project);
	if (serializer.serialize(path))
	{
		s_active_project->m_project_path = path;
		s_active_project->m_project_directory = path.parent_path();
		return true;
	}
	return false;
}

_WHIP_END
