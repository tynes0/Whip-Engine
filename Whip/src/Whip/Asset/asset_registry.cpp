#include "whippch.h"
#include "asset_registry.h"

#include "utils.h"

#include <Whip/Project/project.h>

#include <fstream>

_WHIP_START

static asset_registry::iterator null_iterator = asset_registry::iterator();
static asset_registry::const_iterator null_const_iterator = asset_registry::const_iterator();
static asset_metadata s_null_metadata;

#define NULL_PIT std::pair<asset_registry::private_iterator, bool>{ asset_registry::private_iterator(), false }
#define NULL_PCIT std::pair<asset_registry::private_const_iterator, bool>{ asset_registry::private_const_iterator(), false }

static uint16_t idx(asset_type type)
{
	uint16_t index = static_cast<uint16_t>(type);
	WHP_CORE_ASSERT(index < g_asset_type_count);
	return index;
}

bool asset_registry::add(asset_handle handle, const asset_metadata& metadata)
{
	if (exist(metadata.type, handle))
		return false; // already exists
	if (path_exist(metadata.type, metadata.filepath))
		return false;
	m_registries[idx(metadata.type)][handle] = metadata;
	return true;
}

bool asset_registry::add_or_reset(asset_handle handle, const asset_metadata& metadata)
{
	if (path_exist(metadata.type, metadata.filepath))
		return false;
	m_registries[idx(metadata.type)][handle] = metadata;
	return true;
}

bool asset_registry::remove(asset_handle handle)
{
	auto it = this->private_find(handle);
	if (!it.second)
		return false;
	m_registries[idx(it.first->second.type)].erase(it.first);
	return true;
}

bool asset_registry::remove(asset_type type, asset_handle handle)
{
	auto it = this->private_find(type, handle);
	if (!it.second)
		return false;
	m_registries[idx(type)].erase(it.first);
	return true;
}

void asset_registry::clear()
{
	for (auto& reg : m_registries)
		reg.clear();
}

void asset_registry::clear(asset_type type)
{
	m_registries[idx(type)].clear();
}

filtered_asset_registry& asset_registry::get_filtered(asset_type type)
{
	return m_registries[idx(type)];
}

const filtered_asset_registry& asset_registry::get_filtered(asset_type type) const
{
	return m_registries[idx(type)];
}

asset_metadata& asset_registry::get(asset_handle handle)
{
	iterator it = this->find(handle);
	WHP_CORE_ASSERT(!is_null_it(it), "[Asset Registry] Asset is not exist!");
	return it->second;
}

const asset_metadata& asset_registry::get(asset_handle handle) const
{
	const_iterator it = this->find(handle);
	if (is_null_it(it))
		return s_null_metadata;
	return it->second;
}

asset_metadata& asset_registry::get(asset_type type, asset_handle handle)
{
	iterator it = this->find(type, handle);
	WHP_CORE_ASSERT(!is_null_it(it), "[Asset Registry] Asset is not exist!");
	return it->second;
}

const asset_metadata& asset_registry::get(asset_type type, asset_handle handle) const
{
	const_iterator it = this->find(type, handle);
	if (is_null_it(it))
		return s_null_metadata;
	return it->second;
}

asset_registry::iterator asset_registry::find(asset_handle handle)
{
	if (handle == 0)
		return null_iterator;
	for (auto& reg : m_registries)
	{
		private_iterator it = reg.find(handle);
		if (it != reg.end())
			return iterator(it);
	}
	return null_iterator;
}

asset_registry::const_iterator asset_registry::find(asset_handle handle) const
{
	if (handle == 0)
		return null_const_iterator;
	for (auto& reg : m_registries)
	{
		private_const_iterator it = reg.find(handle);
		if (it != reg.end())
			return const_iterator(it);
	}
	return null_const_iterator;
}

asset_registry::iterator asset_registry::find(asset_type type, asset_handle handle)
{
	if (handle == 0)
		return null_iterator;
	auto& reg = m_registries[idx(type)];
	private_iterator it = reg.find(handle);
	if (it != reg.end())
		return iterator(it);
	return null_iterator;
}

asset_registry::const_iterator asset_registry::find(asset_type type, asset_handle handle) const
{
	if (handle == 0)
		return null_const_iterator;
	auto& reg = m_registries[idx(type)];
	private_const_iterator it = reg.find(handle);
	if (it != reg.end())
		return const_iterator(it);
	return null_const_iterator;
}

bool asset_registry::exist(asset_handle handle) const
{
	return !is_null_it(find(handle));
}

bool asset_registry::exist(asset_type type, asset_handle handle) const
{
	return !is_null_it(find(type, handle));
}

asset_type asset_registry::type_of(asset_handle handle) const
{
	const_iterator it = find(handle);
	if (is_null_it(it))
		return asset_type::none;
	return it->second.type;
}

bool asset_registry::serialize() const
{
	auto path = project::get_active_asset_registry_path();

	YAML::Emitter out;
	{
		out << YAML::BeginMap; // Root
		out << YAML::Key << "asset_registry" << YAML::Value;

		out << YAML::BeginSeq;
		foreach([&out](const value_type& value) 
			{
				out << YAML::BeginMap;
				out << YAML::Key << "handle" << YAML::Value << value.first;
				std::string filepath_str = value.second.filepath.generic_string();
				out << YAML::Key << "filepath" << YAML::Value << filepath_str;
				out << YAML::Key << "type" << YAML::Value << asset_type_to_string(value.second.type);
				out << YAML::EndMap;
			});
		out << YAML::EndSeq;
		out << YAML::EndMap; // Root
	}

	std::ofstream fout(path);
	if (!fout)
	{
		WHP_CORE_WARN("[Asset Registry] Error while opening file '{0}'", path.string());
		return false;
	}
	fout << out.c_str();
	return true;
}

bool asset_registry::deserialize()
{
	auto path = project::get_active_asset_registry_path();
	YAML::Node data;
	try
	{
		data = YAML::LoadFile(path.string());
	}
	catch (YAML::Exception e)
	{
		WHP_CORE_ERROR("[Asset Registry] Failed to load project file '{0}'\n\t{1}", path.string(), e.what());
		return false;
	}

	auto root_node = data["asset_registry"];
	if (!root_node)
		return false;

	for (const auto& node : root_node)
	{
		asset_handle handle = node["handle"].as<uint64_t>();
		asset_metadata metadata;
		metadata.filepath = node["filepath"].as<std::string>();
		metadata.type = asset_type_from_string(node["type"].as<std::string>());
		add_or_reset(handle, metadata);
	}

	return true;
}

bool asset_registry::is_null_it(iterator it) const
{
	return bool(!it.operator->());
}

bool asset_registry::is_null_it(const_iterator it) const
{
	return bool(!it.operator->());
}

void asset_registry::foreach(const std::function<void(value_type&)>& pred)
{
	for (auto& reg : m_registries)
		for (auto& reg_item : reg)
			pred(reg_item);
}

void asset_registry::foreach(const std::function<void(const value_type&)>& pred) const
{
	for (const auto& reg : m_registries)
		for (const auto& reg_item : reg)
			pred(reg_item);
}

void asset_registry::foreach(asset_type type, const std::function<void(value_type&)>& pred)
{
	auto& reg = m_registries[idx(type)];
	for (auto& reg_item : reg)
		pred(reg_item);
}

void asset_registry::foreach(asset_type type, const std::function<void(const value_type&)>& pred) const
{
	const auto& reg = m_registries[idx(type)];
	for (const auto& reg_item : reg)
		pred(reg_item);
}

void asset_registry::foreach_checked(const std::function<uint8_t(value_type&)>& pred)
{
	for (auto& reg : m_registries)
		for (auto& reg_item : reg)
			if (pred(reg_item) & loop_stop)
				return;
}

void asset_registry::foreach_checked(const std::function<uint8_t(const value_type&)>& pred) const
{
	for (const auto& reg : m_registries)
		for (const auto& reg_item : reg)
			if (pred(reg_item) & loop_stop)
				return;
}

void asset_registry::foreach_checked(asset_type type, const std::function<uint8_t(value_type&)>& pred)
{
	auto& reg = m_registries[idx(type)];
	for (auto& reg_item : reg)
		if (pred(reg_item) & loop_stop)
			return;
}

void asset_registry::foreach_checked(asset_type type, const std::function<uint8_t(const value_type&)>& pred) const
{
	const auto& reg = m_registries[idx(type)];
	for (const auto& reg_item : reg)
		if (pred(reg_item) & loop_stop)
			return;
}

std::pair<asset_registry::private_iterator, bool> asset_registry::private_find(asset_handle handle)
{
	if (handle == 0)
		return NULL_PIT;
	for (auto& reg : m_registries)
	{
		private_iterator it = reg.find(handle);
		if (it != reg.end())
			return { it, true };
	}
	return NULL_PIT;
}

std::pair<asset_registry::private_const_iterator, bool> asset_registry::private_find(asset_handle handle) const
{
	if (handle == 0)
		return NULL_PCIT;
	for (auto& reg : m_registries)
	{
		private_const_iterator it = reg.find(handle);
		if (it != reg.end())
			return { it, true };
	}
	return NULL_PCIT;
}

std::pair<asset_registry::private_iterator, bool> asset_registry::private_find(asset_type type, asset_handle handle)
{
	if (handle == 0)
		return NULL_PIT;
	auto& reg = m_registries[idx(type)];
	private_iterator it = reg.find(handle);
	if (it != reg.end())
		return { it, true };
	return NULL_PIT;
}

std::pair<asset_registry::private_const_iterator, bool> asset_registry::private_find(asset_type type, asset_handle handle) const
{
	if (handle == 0)
		return NULL_PCIT;
	auto& reg = m_registries[idx(type)];
	private_const_iterator it = reg.find(handle);
	if (it != reg.end())
		return { it, true };
	return NULL_PCIT;
}

bool asset_registry::path_exist(const std::filesystem::path& filepath) const
{
	for(const auto& filtered_ar : m_registries)
		for (const auto& data : filtered_ar)
			if (data.second.filepath == filepath)
				return true;
	return false;
}

bool asset_registry::path_exist(asset_type type, const std::filesystem::path& filepath) const
{
	const auto& filtered_ar = m_registries[idx(type)];
	for (const auto& data : filtered_ar)
		if (data.second.filepath == filepath)
			return true;
	return false;
}

_WHIP_END
