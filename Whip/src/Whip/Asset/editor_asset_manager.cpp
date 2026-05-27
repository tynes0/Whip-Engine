#include "whippch.h"
#include "editor_asset_manager.h"

#include "asset_importer.h"
#include "utils.h"

#include <Whip/Project/project.h>

#include <fstream>
#include <vector>

_WHIP_START

namespace
{
	bool path_is_under_directory(const std::filesystem::path& path, const std::filesystem::path& directory)
	{
		std::filesystem::path normalized_path = path.lexically_normal();
		std::filesystem::path normalized_directory = directory.lexically_normal();
		if (normalized_path == normalized_directory)
			return true;

		auto path_it = normalized_path.begin();
		auto directory_it = normalized_directory.begin();
		for (; directory_it != normalized_directory.end(); ++directory_it, ++path_it)
		{
			if (path_it == normalized_path.end() || *path_it != *directory_it)
				return false;
		}
		return true;
	}
}

ref<asset> editor_asset_manager::get_asset(asset_handle handle)
{
	if (!is_asset_handle_valid(handle))
		return nullptr;

	ref<asset> p_asset;

	if (is_asset_loaded(handle))
	{
		p_asset = m_loaded_assets.at(handle);
	}
	else
	{
		const asset_metadata& metadata = get_metadata(handle);
		p_asset = asset_importer::import_asset(handle, metadata);
		if (!p_asset)
			WHP_CORE_ERROR("[Asset Manager] Asset import failed!");
		else
			m_loaded_assets[handle] = p_asset;
	}
	return p_asset;
}

bool editor_asset_manager::is_asset_handle_valid(asset_handle handle) const
{
	return m_asset_registry.exist(handle);
}

bool editor_asset_manager::is_asset_loaded(asset_handle handle) const
{
	return m_loaded_assets.find(handle) != m_loaded_assets.end();
}

asset_type editor_asset_manager::get_asset_type(asset_handle handle) const
{
	return m_asset_registry.type_of(handle);
}

const asset_metadata& editor_asset_manager::get_metadata(asset_handle handle) const
{
	return m_asset_registry.get(handle);
}

void editor_asset_manager::add_registry(asset_handle handle, const asset_metadata& metadata)
{
	m_asset_registry.add(handle, metadata);
}

asset_handle editor_asset_manager::import_asset(const std::filesystem::path& filepath)
{
	if (asset_handle existing_handle = get_handle_from_filepath(filepath); existing_handle != 0)
		return existing_handle;

	std::filesystem::path absolute_path = project::get_active_asset_directory() / filepath;
	if (!std::filesystem::exists(absolute_path))
	{
		WHP_CORE_WARN("[Asset Manager] Asset file does not exist: {0}", absolute_path.string());
		return 0;
	}

	asset_type type = utils::try_get_asset_type_from_file_extension(filepath.extension());
	if (type == asset_type::none)
	{
		WHP_CORE_WARN("[Asset Manager] Unsupported asset extension '{0}' for '{1}'", filepath.extension().string(), filepath.string());
		return 0;
	}

	asset_handle handle; // generate new handle
	asset_metadata metadata;
	metadata.filepath = filepath;
	metadata.type = type;
	ref<asset> l_asset = asset_importer::import_asset(handle, metadata);
	if (l_asset)
	{
		l_asset->handle = handle;
		m_loaded_assets[handle] = l_asset;
		m_asset_registry.add_or_reset(handle, metadata);
		m_asset_registry.serialize();
		return handle;
	}
	else
		WHP_CORE_ERROR("[Asset Manager] Asset import failed!");

	return 0;
}

void editor_asset_manager::delete_asset(asset_handle handle)
{
	auto it1 = m_loaded_assets.find(handle);
	if (it1 != m_loaded_assets.end())
		m_loaded_assets.erase(it1);

	m_asset_registry.remove(handle);
	m_asset_registry.serialize();
}

bool editor_asset_manager::update_asset_filepath(asset_handle handle, const std::filesystem::path& filepath)
{
	if (!is_asset_handle_valid(handle))
		return false;

	m_asset_registry.get(handle).filepath = filepath.lexically_normal();
	m_asset_registry.serialize();
	return true;
}

size_t editor_asset_manager::update_asset_directory_paths(const std::filesystem::path& old_directory, const std::filesystem::path& new_directory)
{
	size_t updated_count = 0;
	const std::filesystem::path normalized_old_directory = old_directory.lexically_normal();
	const std::filesystem::path normalized_new_directory = new_directory.lexically_normal();

	m_asset_registry.foreach([&](asset_registry::value_type& value)
		{
			if (!path_is_under_directory(value.second.filepath, normalized_old_directory))
				return;

			std::error_code error;
			std::filesystem::path relative_tail = std::filesystem::relative(value.second.filepath, normalized_old_directory, error);
			if (error)
				return;

			value.second.filepath = (normalized_new_directory / relative_tail).lexically_normal();
			++updated_count;
		});

	if (updated_count > 0)
		m_asset_registry.serialize();
	return updated_count;
}

size_t editor_asset_manager::delete_assets_under_directory(const std::filesystem::path& directory)
{
	std::vector<asset_handle> handles;
	const std::filesystem::path normalized_directory = directory.lexically_normal();
	m_asset_registry.foreach([&](const asset_registry::value_type& value)
		{
			if (path_is_under_directory(value.second.filepath, normalized_directory))
				handles.push_back(value.first);
		});

	for (asset_handle handle : handles)
	{
		auto loaded_it = m_loaded_assets.find(handle);
		if (loaded_it != m_loaded_assets.end())
			m_loaded_assets.erase(loaded_it);
		m_asset_registry.remove(handle);
	}

	if (!handles.empty())
		m_asset_registry.serialize();
	return handles.size();
}

asset_handle editor_asset_manager::get_handle_from_filepath(const std::filesystem::path& filepath) const
{
	asset_handle handle = 0;
	m_asset_registry.foreach_checked([&](const asset_registry::value_type& value)
		{
			if (value.second.filepath == filepath)
			{
				handle = value.first;
				return asset_registry::loop_stop;
			}

			return asset_registry::loop_continue;
		});
	return handle;
}

const std::filesystem::path& editor_asset_manager::get_filepath(asset_handle handle) const
{
	return get_metadata(handle).filepath;
}

void editor_asset_manager::serialize_asset_registry()
{
	m_asset_registry.serialize();
}

bool editor_asset_manager::deserialize_asset_registry()
{
	return m_asset_registry.deserialize();
}

_WHIP_END
