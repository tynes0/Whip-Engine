#include "WhipPch.h"
#include <Whip/Asset/AssetRegistry.h>

#include <Whip/Asset/AssetUtils.h>

#include <Whip/Utils/FileExtensions.h>
#include <Whip/Project/Project.h>

#include <fstream>
#include <string_view>

_WHIP_START

static AssetRegistry::Iterator NullIterator = AssetRegistry::Iterator();
static AssetRegistry::ConstIterator NullConstIterator = AssetRegistry::ConstIterator();
static AssetMetadata s_NullMetadata;

#define NULL_PIT std::pair<AssetRegistry::PrivateIterator, bool>{ AssetRegistry::PrivateIterator(), false }
#define NULL_PCIT std::pair<AssetRegistry::PrivateConstIterator, bool>{ AssetRegistry::PrivateConstIterator(), false }

static bool HasYamlNode(const YAML::Node& node)
{
	return node.IsDefined() && !node.IsNull();
}

static YAML::Node FindYamlValue(const YAML::Node& mapNode, const char* key)
{
	if (!mapNode.IsDefined() || !mapNode.IsMap())
		return {};

	for (const auto& entry : mapNode)
	{
		try
		{
			if (entry.first.as<std::string>() == key)
				return entry.second;
		}
		catch (const YAML::Exception&)
		{
		}
	}

	return {};
}

static YAML::Node FindYamlValue(const YAML::Node& mapNode, const char* firstKey, const char* secondKey)
{
	YAML::Node value = FindYamlValue(mapNode, firstKey);
	if (!HasYamlNode(value))
		value = FindYamlValue(mapNode, secondKey);
	return value;
}

static AssetType ParseAssetType(std::string_view typeName)
{
	if (auto type = frenum::cast<AssetType>(typeName))
		return type.value();

	if (typeName == "none") return AssetType::None;
	if (typeName == "scene") return AssetType::Scene;
	if (typeName == "texture2D") return AssetType::Texture2D;
	if (typeName == "texture2d") return AssetType::Texture2D;
	if (typeName == "audio") return AssetType::Audio;
	if (typeName == "font") return AssetType::Font;
	if (typeName == "animation") return AssetType::Animation;
	if (typeName == "animationController") return AssetType::AnimationController;
	if (typeName == "animationcontroller") return AssetType::AnimationController;
	if (typeName == "entity") return AssetType::Entity;

	WHP_CORE_WARN("[Asset Registry] Unknown AssetType '{0}'", typeName);
	return AssetType::None;
}


bool AssetRegistry::Add(AssetHandle handle, const AssetMetadata& metadata)
{
	if (Exist(metadata.m_Type, handle))
		return false; // already exists
	if (PathExist(metadata.m_Type, metadata.m_Filepath))
		return false;
	m_Registries[*frenum::index(metadata.m_Type)][handle] = metadata;
	return true;
}

bool AssetRegistry::AddOrReset(AssetHandle handle, const AssetMetadata& metadata)
{
	if (PathExist(metadata.m_Type, metadata.m_Filepath))
		return false;
	m_Registries[*frenum::index(metadata.m_Type)][handle] = metadata;
	return true;
}

bool AssetRegistry::Remove(AssetHandle handle)
{
	auto it = this->PrivateFind(handle);
	if (!it.second)
		return false;
	m_Registries[*frenum::index(it.first->second.m_Type)].erase(it.first);
	return true;
}

bool AssetRegistry::Remove(AssetType type, AssetHandle handle)
{
	auto it = this->PrivateFind(type, handle);
	if (!it.second)
		return false;
	m_Registries[*frenum::index(type)].erase(it.first);
	return true;
}

void AssetRegistry::Clear()
{
	for (auto& reg : m_Registries)
		reg.clear();
}

void AssetRegistry::Clear(AssetType type)
{
	m_Registries[*frenum::index(type)].clear();
}

FilteredAssetRegistry& AssetRegistry::GetFiltered(AssetType type)
{
	return m_Registries[*frenum::index(type)];
}

const FilteredAssetRegistry& AssetRegistry::GetFiltered(AssetType type) const
{
	return m_Registries[*frenum::index(type)];
}

AssetMetadata& AssetRegistry::Get(AssetHandle handle)
{
	Iterator it = this->Find(handle);
	WHP_CORE_ASSERT(!IsNullIt(it), "[Asset Registry] Asset is not exist!");
	return it->second;
}

const AssetMetadata& AssetRegistry::Get(AssetHandle handle) const
{
	ConstIterator it = this->Find(handle);
	if (IsNullIt(it))
		return s_NullMetadata;
	return it->second;
}

AssetMetadata& AssetRegistry::Get(AssetType type, AssetHandle handle)
{
	Iterator it = this->Find(type, handle);
	WHP_CORE_ASSERT(!IsNullIt(it), "[Asset Registry] Asset is not exist!");
	return it->second;
}

const AssetMetadata& AssetRegistry::Get(AssetType type, AssetHandle handle) const
{
	ConstIterator it = this->Find(type, handle);
	if (IsNullIt(it))
		return s_NullMetadata;
	return it->second;
}

AssetRegistry::Iterator AssetRegistry::Find(AssetHandle handle)
{
	if (handle == 0)
		return NullIterator;
	for (auto& reg : m_Registries)
	{
		PrivateIterator it = reg.find(handle);
		if (it != reg.end())
			return Iterator(it);
	}
	return NullIterator;
}

AssetRegistry::ConstIterator AssetRegistry::Find(AssetHandle handle) const
{
	if (handle == 0)
		return NullConstIterator;
	for (auto& reg : m_Registries)
	{
		PrivateConstIterator it = reg.find(handle);
		if (it != reg.end())
			return ConstIterator(it);
	}
	return NullConstIterator;
}

AssetRegistry::Iterator AssetRegistry::Find(AssetType type, AssetHandle handle)
{
	if (handle == 0)
		return NullIterator;
	auto& reg = m_Registries[*frenum::index(type)];
	PrivateIterator it = reg.find(handle);
	if (it != reg.end())
		return Iterator(it);
	return NullIterator;
}

AssetRegistry::ConstIterator AssetRegistry::Find(AssetType type, AssetHandle handle) const
{
	if (handle == 0)
		return NullConstIterator;
	auto& reg = m_Registries[*frenum::index(type)];
	PrivateConstIterator it = reg.find(handle);
	if (it != reg.end())
		return ConstIterator(it);
	return NullConstIterator;
}

bool AssetRegistry::Exist(AssetHandle handle) const
{
	return !IsNullIt(Find(handle));
}

bool AssetRegistry::Exist(AssetType type, AssetHandle handle) const
{
	return !IsNullIt(Find(type, handle));
}

AssetType AssetRegistry::TypeOf(AssetHandle handle) const
{
	ConstIterator it = Find(handle);
	if (IsNullIt(it))
		return AssetType::None;
	return it->second.m_Type;
}

bool AssetRegistry::Serialize() const
{
	auto path = Project::GetActiveAssetRegistryPath();

	YAML::Emitter out;
	{
		out << YAML::BeginMap; // Root
		out << YAML::Key << "asset_registry" << YAML::Value;

		out << YAML::BeginSeq;
		Foreach([&out](const ValueType& value) 
			{
				out << YAML::BeginMap;
				out << YAML::Key << "handle" << YAML::Value << value.first;
				std::string filepathString = value.second.m_Filepath.generic_string();
				out << YAML::Key << "filepath" << YAML::Value << filepathString;
				out << YAML::Key << "type" << YAML::Value << frenum::to_string(value.second.m_Type);
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

bool AssetRegistry::Deserialize()
{
	auto path = Project::GetActiveAssetRegistryPath();
	std::error_code fileError;
	if (!std::filesystem::exists(path, fileError) && FileExtensions::ExtensionEquals(path, FileExtensions::AssetRegistry))
	{
		std::filesystem::path legacyPath = path;
		legacyPath.replace_extension(FileExtensions::AssetRegistryLegacy);
		fileError.clear();
		if (std::filesystem::exists(legacyPath, fileError))
			path = legacyPath;
	}

	YAML::Node data;
	try
	{
		data = YAML::LoadFile(path.string());
	}
	catch (YAML::Exception e)
	{
		WHP_CORE_ERROR("[Asset Registry] Failed to load Project file '{0}'\n\t{1}", path.string(), e.what());
		return false;
	}

	auto rootNode = FindYamlValue(data, "asset_registry", "AssetRegistry");
	if (!HasYamlNode(rootNode))
		return false;

	Clear();
	for (const auto& node : rootNode)
	{
		try
		{
			YAML::Node handleNode = FindYamlValue(node, "handle", "Handle");
			YAML::Node filepathNode = FindYamlValue(node, "filepath", "Filepath");
			YAML::Node typeNode = FindYamlValue(node, "type", "Type");

			AssetHandle handle = handleNode.as<uint64_t>();
			AssetMetadata metadata;
			metadata.m_Filepath = filepathNode.as<std::string>();
			metadata.m_Type = ParseAssetType(typeNode.as<std::string>());
			AddOrReset(handle, metadata);
		}
		catch (const YAML::Exception& e)
		{
			WHP_CORE_WARN("[Asset Registry] Skipping invalid registry entry in '{0}': {1}", path.string(), e.what());
		}
	}

	return true;
}

bool AssetRegistry::IsNullIt(Iterator it) const
{
	return bool(!it.operator->());
}

bool AssetRegistry::IsNullIt(ConstIterator it) const
{
	return bool(!it.operator->());
}

void AssetRegistry::Foreach(const std::function<void(ValueType&)>& pred)
{
	for (auto& reg : m_Registries)
		for (auto& registryItem : reg)
			pred(registryItem);
}

void AssetRegistry::Foreach(const std::function<void(const ValueType&)>& pred) const
{
	for (const auto& reg : m_Registries)
		for (const auto& registryItem : reg)
			pred(registryItem);
}

void AssetRegistry::Foreach(AssetType type, const std::function<void(ValueType&)>& pred)
{
	auto& reg = m_Registries[*frenum::index(type)];
	for (auto& registryItem : reg)
		pred(registryItem);
}

void AssetRegistry::Foreach(AssetType type, const std::function<void(const ValueType&)>& pred) const
{
	const auto& reg = m_Registries[*frenum::index(type)];
	for (const auto& registryItem : reg)
		pred(registryItem);
}

void AssetRegistry::ForeachChecked(const std::function<uint8_t(ValueType&)>& pred)
{
	for (auto& reg : m_Registries)
		for (auto& registryItem : reg)
			if (pred(registryItem) & LoopStop)
				return;
}

void AssetRegistry::ForeachChecked(const std::function<uint8_t(const ValueType&)>& pred) const
{
	for (const auto& reg : m_Registries)
		for (const auto& registryItem : reg)
			if (pred(registryItem) & LoopStop)
				return;
}

void AssetRegistry::ForeachChecked(AssetType type, const std::function<uint8_t(ValueType&)>& pred)
{
	auto& reg = m_Registries[*frenum::index(type)];
	for (auto& registryItem : reg)
		if (pred(registryItem) & LoopStop)
			return;
}

void AssetRegistry::ForeachChecked(AssetType type, const std::function<uint8_t(const ValueType&)>& pred) const
{
	const auto& reg = m_Registries[*frenum::index(type)];
	for (const auto& registryItem : reg)
		if (pred(registryItem) & LoopStop)
			return;
}

std::pair<AssetRegistry::PrivateIterator, bool> AssetRegistry::PrivateFind(AssetHandle handle)
{
	if (handle == 0)
		return NULL_PIT;
	for (auto& reg : m_Registries)
	{
		PrivateIterator it = reg.find(handle);
		if (it != reg.end())
			return { it, true };
	}
	return NULL_PIT;
}

std::pair<AssetRegistry::PrivateConstIterator, bool> AssetRegistry::PrivateFind(AssetHandle handle) const
{
	if (handle == 0)
		return NULL_PCIT;
	for (auto& reg : m_Registries)
	{
		PrivateConstIterator it = reg.find(handle);
		if (it != reg.end())
			return { it, true };
	}
	return NULL_PCIT;
}

std::pair<AssetRegistry::PrivateIterator, bool> AssetRegistry::PrivateFind(AssetType type, AssetHandle handle)
{
	if (handle == 0)
		return NULL_PIT;
	auto& reg = m_Registries[*frenum::index(type)];
	PrivateIterator it = reg.find(handle);
	if (it != reg.end())
		return { it, true };
	return NULL_PIT;
}

std::pair<AssetRegistry::PrivateConstIterator, bool> AssetRegistry::PrivateFind(AssetType type, AssetHandle handle) const
{
	if (handle == 0)
		return NULL_PCIT;
	auto& reg = m_Registries[*frenum::index(type)];
	PrivateConstIterator it = reg.find(handle);
	if (it != reg.end())
		return { it, true };
	return NULL_PCIT;
}

bool AssetRegistry::PathExist(const std::filesystem::path& filepath) const
{
	for(const auto& filteredAssetRegistry : m_Registries)
		for (const auto& data : filteredAssetRegistry)
			if (data.second.m_Filepath == filepath)
				return true;
	return false;
}

bool AssetRegistry::PathExist(AssetType type, const std::filesystem::path& filepath) const
{
	const auto& filteredAssetRegistry = m_Registries[*frenum::index(type)];
	for (const auto& data : filteredAssetRegistry)
		if (data.second.m_Filepath == filepath)
			return true;
	return false;
}

_WHIP_END
