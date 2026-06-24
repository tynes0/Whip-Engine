#include "WhipPch.h"
#include "Whip/Asset/AssetRegistry.h"
#include "Whip/Asset/AssetUtils.h"
#include "Whip/Utils/FileExtensions.h"
#include "Whip/Project/Project.h"

#include <fstream>

_WHIP_START

#define NULL_PIT std::pair<AssetRegistry::PrivateIterator, bool>{ AssetRegistry::PrivateIterator(), false }
#define NULL_PCIT std::pair<AssetRegistry::PrivateConstIterator, bool>{ AssetRegistry::PrivateConstIterator(), false }

namespace
{
	AssetRegistry::Iterator NullIterator = AssetRegistry::Iterator();
	AssetRegistry::ConstIterator NullConstIterator = AssetRegistry::ConstIterator();
	AssetMetadata s_NullMetadata;

	bool HasYamlNode(const YAML::Node& node)
	{
		return node.IsDefined() && !node.IsNull();
	}

	YAML::Node FindYamlValue(const YAML::Node& mapNode, const char* key)
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
				WHP_CORE_WARN("[Asset Registry] Failed yaml value finding!");
			}
		}

		return {};
	}

	YAML::Node FindYamlValue(const YAML::Node& mapNode, const char* firstKey, const char* secondKey)
	{
		YAML::Node value = FindYamlValue(mapNode, firstKey);
		if (!HasYamlNode(value))
			value = FindYamlValue(mapNode, secondKey);
		return value;
	}

	AssetType ParseAssetType(std::string_view typeName)
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

	std::filesystem::path NormalizeRegistryPath(std::filesystem::path filepath)
	{
		if (filepath.empty())
			return {};
		filepath = filepath.lexically_normal();
		if (filepath == ".")
			return {};
		return filepath;
	}

	std::string NormalizeRegistryPathString(const std::filesystem::path& filepath)
	{
		std::string value = NormalizeRegistryPath(filepath).generic_string();
		std::ranges::transform(value, value.begin(),
		                       [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
		return value;
	}

	bool RegistryPathsEqual(const std::filesystem::path& left, const std::filesystem::path& right)
	{
		return NormalizeRegistryPathString(left) == NormalizeRegistryPathString(right);
	}

	std::string LowerCopy(std::string value)
	{
		std::ranges::transform(value, value.begin(),
			[](unsigned char character) { return static_cast<char>(std::tolower(character)); });
		return value;
	}

	const char* TextureFilterModeName(TextureFilterMode mode)
	{
		switch (mode)
		{
		case TextureFilterMode::Nearest: return "Nearest";
		case TextureFilterMode::Linear: return "Linear";
		default: return "Linear";
		}
	}

	const char* TextureWrapModeName(TextureWrapMode mode)
	{
		switch (mode)
		{
		case TextureWrapMode::Repeat: return "Repeat";
		case TextureWrapMode::ClampToEdge: return "ClampToEdge";
		default: return "Repeat";
		}
	}

	const char* TextureSpriteModeName(TextureSpriteMode mode)
	{
		switch (mode)
		{
		case TextureSpriteMode::Single: return "Single";
		case TextureSpriteMode::Multiple: return "Multiple";
		default: return "Single";
		}
	}

	TextureFilterMode ParseTextureFilterMode(std::string value)
	{
		value = LowerCopy(std::move(value));
		if (value == "nearest")
			return TextureFilterMode::Nearest;
		return TextureFilterMode::Linear;
	}

	TextureWrapMode ParseTextureWrapMode(std::string value)
	{
		value = LowerCopy(std::move(value));
		if (value == "clamptoedge" || value == "clamp_to_edge" || value == "clamp")
			return TextureWrapMode::ClampToEdge;
		return TextureWrapMode::Repeat;
	}

	TextureSpriteMode ParseTextureSpriteMode(std::string value)
	{
		value = LowerCopy(std::move(value));
		if (value == "multiple")
			return TextureSpriteMode::Multiple;
		return TextureSpriteMode::Single;
	}

	void SerializeTextureImportSettings(YAML::Emitter& out, const TextureImportSettings& settings)
	{
		out << YAML::Key << "texture_import" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "filter" << YAML::Value << TextureFilterModeName(settings.m_FilterMode);
		out << YAML::Key << "wrap" << YAML::Value << TextureWrapModeName(settings.m_WrapMode);
		out << YAML::Key << "generate_mips" << YAML::Value << settings.m_GenerateMips;
		out << YAML::Key << "alpha_transparency" << YAML::Value << settings.m_AlphaTransparency;
		out << YAML::Key << "sprite_mode" << YAML::Value << TextureSpriteModeName(settings.m_SpriteMode);
		out << YAML::Key << "pixels_per_unit" << YAML::Value << settings.m_PixelsPerUnit;
		out << YAML::Key << "sprites" << YAML::Value << YAML::BeginSeq;
		for (const TextureSpriteRect& sprite : settings.m_Sprites)
		{
			out << YAML::BeginMap;
			out << YAML::Key << "name" << YAML::Value << sprite.m_Name;
			out << YAML::Key << "x" << YAML::Value << sprite.m_X;
			out << YAML::Key << "y" << YAML::Value << sprite.m_Y;
			out << YAML::Key << "width" << YAML::Value << sprite.m_Width;
			out << YAML::Key << "height" << YAML::Value << sprite.m_Height;
			out << YAML::EndMap;
		}
		out << YAML::EndSeq;
		out << YAML::EndMap;
	}

	void DeserializeTextureImportSettings(const YAML::Node& assetNode, TextureImportSettings& settings)
	{
		YAML::Node importNode = FindYamlValue(assetNode, "texture_import", "TextureImport");
		if (!HasYamlNode(importNode) || !importNode.IsMap())
			return;

		if (YAML::Node value = FindYamlValue(importNode, "filter", "Filter"); HasYamlNode(value))
			settings.m_FilterMode = ParseTextureFilterMode(value.as<std::string>());
		if (YAML::Node value = FindYamlValue(importNode, "wrap", "Wrap"); HasYamlNode(value))
			settings.m_WrapMode = ParseTextureWrapMode(value.as<std::string>());
		if (YAML::Node value = FindYamlValue(importNode, "generate_mips", "GenerateMips"); HasYamlNode(value))
			settings.m_GenerateMips = value.as<bool>();
		if (YAML::Node value = FindYamlValue(importNode, "alpha_transparency", "AlphaTransparency"); HasYamlNode(value))
			settings.m_AlphaTransparency = value.as<bool>();
		if (YAML::Node value = FindYamlValue(importNode, "sprite_mode", "SpriteMode"); HasYamlNode(value))
			settings.m_SpriteMode = ParseTextureSpriteMode(value.as<std::string>());
		if (YAML::Node value = FindYamlValue(importNode, "pixels_per_unit", "PixelsPerUnit"); HasYamlNode(value))
			settings.m_PixelsPerUnit = std::max(1.0f, value.as<float>());

		settings.m_Sprites.clear();
		YAML::Node spritesNode = FindYamlValue(importNode, "sprites", "Sprites");
		if (!HasYamlNode(spritesNode) || !spritesNode.IsSequence())
			return;

		for (const YAML::Node& spriteNode : spritesNode)
		{
			if (!spriteNode.IsMap())
				continue;

			TextureSpriteRect sprite;
			if (YAML::Node value = FindYamlValue(spriteNode, "name", "Name"); HasYamlNode(value))
				sprite.m_Name = value.as<std::string>();
			if (YAML::Node value = FindYamlValue(spriteNode, "x", "X"); HasYamlNode(value))
				sprite.m_X = value.as<uint32_t>();
			if (YAML::Node value = FindYamlValue(spriteNode, "y", "Y"); HasYamlNode(value))
				sprite.m_Y = value.as<uint32_t>();
			if (YAML::Node value = FindYamlValue(spriteNode, "width", "Width"); HasYamlNode(value))
				sprite.m_Width = std::max<uint32_t>(1, value.as<uint32_t>());
			if (YAML::Node value = FindYamlValue(spriteNode, "height", "Height"); HasYamlNode(value))
				sprite.m_Height = std::max<uint32_t>(1, value.as<uint32_t>());
			if (sprite.m_Name.empty())
				sprite.m_Name = "Sprite";
			settings.m_Sprites.push_back(std::move(sprite));
		}

		Utils::NormalizeTextureSprites(settings);
	}
}

RegistryIterator::RegistryIterator()
	: m_Data(nullptr)
{
}

RegistryIterator::RegistryIterator(ValueType* data)
	: m_Data(data)
{
}

RegistryIterator::RegistryIterator(const FilteredAssetRegistry::iterator& it)
	: m_Data(it.operator->())
{
}

RegistryIterator::ValueType& RegistryIterator::operator*()
{
	return *m_Data;
}

const RegistryIterator::ValueType& RegistryIterator::operator*() const
{
	return *m_Data;
}

RegistryIterator::ValueType* RegistryIterator::operator->()
{
	return m_Data;
}

const RegistryIterator::ValueType* RegistryIterator::operator->() const
{
	return m_Data;
}

bool RegistryIterator::operator==(RegistryIterator it) const
{ return m_Data == it.m_Data; }

bool RegistryIterator::operator!=(RegistryIterator it) const
{ return !this->operator==(it); }

RegistryConstIterator::RegistryConstIterator()
	: m_Data(nullptr)
{
}

RegistryConstIterator::RegistryConstIterator(const ValueType* data)
	: m_Data(data)
{
}

RegistryConstIterator::RegistryConstIterator(const FilteredAssetRegistry::const_iterator& it)
	: m_Data(it.operator->())
{
}

const RegistryConstIterator::ValueType& RegistryConstIterator::operator*() const
{
	return *m_Data;
}

const RegistryConstIterator::ValueType* RegistryConstIterator::operator->() const
{
	return m_Data;
}

bool RegistryConstIterator::operator==(RegistryConstIterator it) const
{
	return m_Data == it.m_Data;
}

bool RegistryConstIterator::operator!=(RegistryConstIterator it) const
{
	return !this->operator==(it);
}

bool AssetRegistry::Add(AssetHandle handle, const AssetMetadata& metadata)
{
	AssetMetadata normalizedMetadata = metadata;
	normalizedMetadata.m_Filepath = NormalizeRegistryPath(metadata.m_Filepath);
	if (normalizedMetadata.m_Filepath.empty() || normalizedMetadata.m_Type == AssetType::None)
		return false;
	if (Exist(normalizedMetadata.m_Type, handle))
		return false; // already exists
	if (PathExist(normalizedMetadata.m_Type, normalizedMetadata.m_Filepath))
		return false;
	m_Registries[*frenum::index(normalizedMetadata.m_Type)][handle] = normalizedMetadata;
	return true;
}

bool AssetRegistry::AddOrReset(AssetHandle handle, const AssetMetadata& metadata)
{
	AssetMetadata normalizedMetadata = metadata;
	normalizedMetadata.m_Filepath = NormalizeRegistryPath(metadata.m_Filepath);
	if (normalizedMetadata.m_Filepath.empty() || normalizedMetadata.m_Type == AssetType::None)
		return false;
	const auto& filteredAssetRegistry = m_Registries[*frenum::index(normalizedMetadata.m_Type)];
	for (const auto& data : filteredAssetRegistry)
		if (data.first != handle && RegistryPathsEqual(data.second.m_Filepath, normalizedMetadata.m_Filepath))
			return false;

	const AssetType existingType = TypeOf(handle);
	if (existingType != AssetType::None && existingType != normalizedMetadata.m_Type)
		Remove(existingType, handle);

	m_Registries[*frenum::index(normalizedMetadata.m_Type)][handle] = normalizedMetadata;
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
			return Iterator{ it };
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
			return ConstIterator{ it };
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
		return Iterator{ it };
	return NullIterator;
}

AssetRegistry::ConstIterator AssetRegistry::Find(AssetType type, AssetHandle handle) const
{
	if (handle == 0)
		return NullConstIterator;
	auto& reg = m_Registries[*frenum::index(type)];
	PrivateConstIterator it = reg.find(handle);
	if (it != reg.end())
		return ConstIterator{ it };
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
				if (value.second.m_Type == AssetType::Texture2D)
					SerializeTextureImportSettings(out, value.second.m_TextureSettings);
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
	catch (const YAML::Exception& e)
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
			metadata.m_Filepath = NormalizeRegistryPath(filepathNode.as<std::string>());
			metadata.m_Type = ParseAssetType(typeNode.as<std::string>());
			if (metadata.m_Type == AssetType::Texture2D)
				DeserializeTextureImportSettings(node, metadata.m_TextureSettings);
			AddOrReset(handle, metadata);
		}
		catch (const YAML::Exception& e)
		{
			WHP_CORE_WARN("[Asset Registry] Skipping invalid registry entry in '{0}': {1}", path.string(), e.what());
		}
	}

	return true;
}

bool AssetRegistry::IsNullIt(Iterator it)
{
	return static_cast<bool>(!it.operator->());
}

bool AssetRegistry::IsNullIt(ConstIterator it)
{
	return static_cast<bool>(!it.operator->());
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
			if (RegistryPathsEqual(data.second.m_Filepath, filepath))
				return true;
	return false;
}

bool AssetRegistry::PathExist(AssetType type, const std::filesystem::path& filepath) const
{
	const auto& filteredAssetRegistry = m_Registries[*frenum::index(type)];
	for (const auto& data : filteredAssetRegistry)
		if (RegistryPathsEqual(data.second.m_Filepath, filepath))
			return true;
	return false;
}

_WHIP_END
