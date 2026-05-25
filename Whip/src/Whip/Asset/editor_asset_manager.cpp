#include "whippch.h"
#include "editor_asset_manager.h"

#include "asset_importer.h"
#include "utils.h"

#include <Whip/Project/project.h>

#include <fstream>

_WHIP_START

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

	asset_type type = utils::get_asset_type_from_file_extension(filepath.extension());
	if (type == asset_type::none)
		return 0;

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
