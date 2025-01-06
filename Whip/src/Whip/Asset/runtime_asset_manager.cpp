#include <whippch.h>
#include "runtime_asset_manager.h" 

#include "asset_importer.h"
#include "utils.h"

#include <fstream>

_WHIP_START

ref<asset> runtime_asset_manager::get_asset(asset_handle handle)
{
	if (!is_runtime(handle))
		return m_editor_asset_manager->get_asset(handle);

	if (is_asset_loaded(handle))
		return m_loaded_assets.at(handle);
	const asset_metadata& metadata = get_metadata(handle);
	ref<asset> p_asset = asset_importer::import_asset(handle, metadata);
	if (!p_asset)
		WHP_CORE_ERROR("[Asset Manager] Asset import failed!");
	m_loaded_assets[handle] = p_asset;
	return p_asset;
}

bool runtime_asset_manager::is_asset_handle_valid(asset_handle handle) const
{
	if (m_asset_registry.exist(handle))
		return true;
	return m_editor_asset_manager->is_asset_handle_valid(handle);
}

bool runtime_asset_manager::is_asset_loaded(asset_handle handle) const
{
	if (m_loaded_assets.find(handle) != m_loaded_assets.end())
		return true;
	return m_editor_asset_manager->is_asset_loaded(handle);
}

asset_type runtime_asset_manager::get_asset_type(asset_handle handle) const
{
	if (is_runtime(handle))
		return m_asset_registry.type_of(handle);
	return m_editor_asset_manager->get_asset_type(handle);
}

void runtime_asset_manager::add_registry(asset_handle handle, const asset_metadata& metadata)
{
	m_asset_registry.add(handle, metadata);
}

asset_handle runtime_asset_manager::import_asset(const std::filesystem::path& filepath)
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
	}
	return handle;
}

void runtime_asset_manager::delete_asset(asset_handle handle)
{
	m_asset_registry.remove(handle);
	auto it1 = m_loaded_assets.find(handle);
	if (it1 != m_loaded_assets.end())
		m_loaded_assets.erase(it1);
}

const asset_metadata& runtime_asset_manager::get_metadata(asset_handle handle) const
{
	auto it = m_asset_registry.find(handle);
	if (m_asset_registry.is_null_it(it))
		return m_editor_asset_manager->get_metadata(handle);
	return it->second;
}

const std::filesystem::path& runtime_asset_manager::get_filepath(asset_handle handle) const
{
	return get_metadata(handle).filepath;
}

void runtime_asset_manager::runtime_stop()
{
	m_asset_registry.clear();
	m_loaded_assets.clear();
}

void runtime_asset_manager::set_editor_asset_manager(ref<editor_asset_manager> manager)
{
	m_editor_asset_manager = manager;
}

void runtime_asset_manager::add_asset_copy(asset_handle handle, ref<asset> copy_of_asset)
{
	if (is_asset_handle_valid(handle))
	{
		m_asset_registry.add_or_reset(copy_of_asset->handle, get_metadata(handle));
		m_loaded_assets[copy_of_asset->handle] = copy_of_asset;
	}
}

bool runtime_asset_manager::is_runtime(asset_handle handle) const
{
	return m_asset_registry.exist(handle);
}

_WHIP_END
