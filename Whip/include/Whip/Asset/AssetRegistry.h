#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/Memory.h>

#include "AssetMetadata.h"
#include "Asset.h"

#include <unordered_map>
#include <array>
#include <functional>

_WHIP_START

using FilteredAssetRegistry = std::unordered_map<AssetHandle, AssetMetadata>;

class RegistryIterator
{
public:
	using ValueType = std::pair<const AssetHandle, AssetMetadata>;

	RegistryIterator() : m_Data(nullptr) {}
	RegistryIterator(ValueType* data) : m_Data(data) {}
	RegistryIterator(FilteredAssetRegistry::iterator it) : m_Data(it.operator->()) {}
	~RegistryIterator() {}

	ValueType& operator*() { return *m_Data; }
	ValueType* operator->() { return m_Data; }

	bool operator==(RegistryIterator it) { return m_Data == it.m_Data; }
	bool operator!=(RegistryIterator it) { return !this->operator==(it); }
private:
	ValueType* m_Data;
};

class RegistryConstIterator
{
public:
	using ValueType = std::pair<const AssetHandle, AssetMetadata>;

	RegistryConstIterator() : m_Data(nullptr) {}
	RegistryConstIterator(ValueType* data) : m_Data(data) {}
	RegistryConstIterator(FilteredAssetRegistry::const_iterator it) : m_Data(it.operator->()) {}
	~RegistryConstIterator() {}

	const ValueType& operator*() const { return *m_Data; }
	const ValueType* operator->() const { return m_Data; }

	bool operator==(RegistryConstIterator it) { return m_Data == it.m_Data; }
	bool operator!=(RegistryConstIterator it) { return !this->operator==(it); }
private:
	const ValueType* m_Data;
};

class AssetRegistry
{
public:
	using ValueType		= FilteredAssetRegistry::value_type;
	using Iterator			= RegistryIterator;
	using ConstIterator	= RegistryConstIterator;

	AssetRegistry() {}
	~AssetRegistry() {}

	bool Add(AssetHandle handle, const AssetMetadata& metadata);
	bool AddOrReset(AssetHandle handle, const AssetMetadata& metadata);
	bool Remove(AssetHandle handle);
	bool Remove(AssetType type, AssetHandle handle);
	void Clear();
	void Clear(AssetType type);

	FilteredAssetRegistry& GetFiltered(AssetType type);
	const FilteredAssetRegistry& GetFiltered(AssetType type) const;

	AssetMetadata& Get(AssetHandle handle);
	const AssetMetadata& Get(AssetHandle handle) const;
	AssetMetadata& Get(AssetType type, AssetHandle handle);
	const AssetMetadata& Get(AssetType type, AssetHandle handle) const;

	Iterator Find(AssetHandle handle);
	ConstIterator Find(AssetHandle handle) const;
	Iterator Find(AssetType type, AssetHandle handle);
	ConstIterator Find(AssetType type, AssetHandle handle) const;

	bool Exist(AssetHandle handle) const;
	bool Exist(AssetType type, AssetHandle handle) const;

	AssetType TypeOf(AssetHandle handle) const;

	bool Serialize() const;
	bool Deserialize();

	bool IsNullIt(Iterator it) const;
	bool IsNullIt(ConstIterator it) const;

	enum : uint8_t { LoopStop, LoopContinue };

	void Foreach(const std::function<void(ValueType&)>& pred);
	void Foreach(const std::function<void(const ValueType&)>& pred) const;
	void Foreach(AssetType type, const std::function<void(ValueType&)>& pred);
	void Foreach(AssetType type, const std::function<void(const ValueType&)>& pred) const;
	void ForeachChecked(const std::function<uint8_t(ValueType&)>& pred);
	void ForeachChecked(const std::function<uint8_t(const ValueType&)>& pred) const;
	void ForeachChecked(AssetType type, const std::function<uint8_t(ValueType&)>& pred);
	void ForeachChecked(AssetType type, const std::function<uint8_t(const ValueType&)>& pred) const;
private:
	using PrivateIterator = FilteredAssetRegistry::iterator;
	using PrivateConstIterator = FilteredAssetRegistry::const_iterator;
	
	std::pair<PrivateIterator, bool> PrivateFind(AssetHandle handle);
	std::pair<PrivateConstIterator, bool> PrivateFind(AssetHandle handle) const;
	std::pair<PrivateIterator, bool> PrivateFind(AssetType type, AssetHandle handle);
	std::pair<PrivateConstIterator, bool> PrivateFind(AssetType type, AssetHandle handle) const;

	bool PathExist(const std::filesystem::path& filepath) const;
	bool PathExist(AssetType type, const std::filesystem::path& filepath) const;

	std::array<FilteredAssetRegistry, frenum::size<AssetType>()> m_Registries;
};

_WHIP_END
