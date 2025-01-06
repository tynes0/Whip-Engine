#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/memory.h>

#include "asset_metadata.h"
#include "asset.h"

#include <unordered_map>
#include <array>
#include <functional>

_WHIP_START

using filtered_asset_registry = std::unordered_map<asset_handle, asset_metadata>;

class registry_iterator
{
public:
	using value_type = std::pair<const asset_handle, asset_metadata>;

	registry_iterator() : m_data(nullptr) {}
	registry_iterator(value_type* data) : m_data(data) {}
	registry_iterator(filtered_asset_registry::iterator it) : m_data(it.operator->()) {}
	~registry_iterator() {}

	value_type& operator*() { return *m_data; }
	value_type* operator->() { return m_data; }

	bool operator==(registry_iterator it) { return m_data == it.m_data; }
	bool operator!=(registry_iterator it) { return !this->operator==(it); }
private:
	value_type* m_data;
};

class registry_const_iterator
{
public:
	using value_type = std::pair<const asset_handle, asset_metadata>;

	registry_const_iterator() : m_data(nullptr) {}
	registry_const_iterator(value_type* data) : m_data(data) {}
	registry_const_iterator(filtered_asset_registry::const_iterator it) : m_data(it.operator->()) {}
	~registry_const_iterator() {}

	const value_type& operator*() const { return *m_data; }
	const value_type* operator->() const { return m_data; }

	bool operator==(registry_const_iterator it) { return m_data == it.m_data; }
	bool operator!=(registry_const_iterator it) { return !this->operator==(it); }
private:
	const value_type* m_data;
};

class asset_registry
{
public:
	using value_type		= filtered_asset_registry::value_type;
	using iterator			= registry_iterator;
	using const_iterator	= registry_const_iterator;

	asset_registry() {}
	~asset_registry() {}

	bool add(asset_handle handle, const asset_metadata& metadata);
	bool add_or_reset(asset_handle handle, const asset_metadata& metadata);
	bool remove(asset_handle handle);
	bool remove(asset_type type, asset_handle handle);
	void clear();
	void clear(asset_type type);

	filtered_asset_registry& get_filtered(asset_type type);
	const filtered_asset_registry& get_filtered(asset_type type) const;

	asset_metadata& get(asset_handle handle);
	const asset_metadata& get(asset_handle handle) const;
	asset_metadata& get(asset_type type, asset_handle handle);
	const asset_metadata& get(asset_type type, asset_handle handle) const;

	iterator find(asset_handle handle);
	const_iterator find(asset_handle handle) const;
	iterator find(asset_type type, asset_handle handle);
	const_iterator find(asset_type type, asset_handle handle) const;

	bool exist(asset_handle handle) const;
	bool exist(asset_type type, asset_handle handle) const;

	asset_type type_of(asset_handle handle) const;

	bool serialize() const;
	bool deserialize();

	bool is_null_it(iterator it) const;
	bool is_null_it(const_iterator it) const;

	enum : uint8_t { loop_stop, loop_continue };

	void foreach(const std::function<void(value_type&)>& pred);
	void foreach(const std::function<void(const value_type&)>& pred) const;
	void foreach(asset_type type, const std::function<void(value_type&)>& pred);
	void foreach(asset_type type, const std::function<void(const value_type&)>& pred) const;
	void foreach_checked(const std::function<uint8_t(value_type&)>& pred);
	void foreach_checked(const std::function<uint8_t(const value_type&)>& pred) const;
	void foreach_checked(asset_type type, const std::function<uint8_t(value_type&)>& pred);
	void foreach_checked(asset_type type, const std::function<uint8_t(const value_type&)>& pred) const;
private:
	using private_iterator = filtered_asset_registry::iterator;
	using private_const_iterator = filtered_asset_registry::const_iterator;
	
	std::pair<private_iterator, bool> private_find(asset_handle handle);
	std::pair<private_const_iterator, bool> private_find(asset_handle handle) const;
	std::pair<private_iterator, bool> private_find(asset_type type, asset_handle handle);
	std::pair<private_const_iterator, bool> private_find(asset_type type, asset_handle handle) const;

	bool path_exist(const std::filesystem::path& filepath) const;
	bool path_exist(asset_type type, const std::filesystem::path& filepath) const;

	std::array<filtered_asset_registry, g_asset_type_count> m_registries;
};

_WHIP_END
