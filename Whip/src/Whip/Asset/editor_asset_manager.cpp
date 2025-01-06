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

void editor_asset_manager::import_asset(const std::filesystem::path& filepath)
{
	asset_handle handle; // generate new handle
	asset_metadata metadata;
	metadata.filepath = filepath;
	metadata.type = utils::get_asset_type_from_file_extension(filepath.extension());
	ref<asset> l_asset = asset_importer::import_asset(handle, metadata);
	if (l_asset)
	{
		l_asset->handle = handle;
		m_loaded_assets[handle] = l_asset;
		m_asset_registry.add_or_reset(handle, metadata);
		m_asset_registry.serialize();
	}
	else
		WHP_CORE_ERROR("[Asset Manager] Asset import failed!");
}

void editor_asset_manager::delete_asset(asset_handle handle)
{
	auto it1 = m_loaded_assets.find(handle);
	if (it1 != m_loaded_assets.end())
		m_loaded_assets.erase(it1);

	m_asset_registry.remove(handle);
	m_asset_registry.serialize();
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
