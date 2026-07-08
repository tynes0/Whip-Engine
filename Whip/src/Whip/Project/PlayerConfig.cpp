#include "WhipPch.h"
#include <Whip/Project/PlayerConfig.h>

#include <algorithm>
#include <fstream>

#include <yaml-cpp/yaml.h>

_WHIP_START

namespace
{
	bool HasNode(const YAML::Node& node)
	{
		return node.IsDefined() && !node.IsNull();
	}

	YAML::Node FindMapValue(const YAML::Node& mapNode, const char* key)
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

	YAML::Node ReadValue(const YAML::Node& node, const char* legacyKey, const char* pascalKey)
	{
		YAML::Node value = FindMapValue(node, legacyKey);
		if (!HasNode(value))
			value = FindMapValue(node, pascalKey);
		return value;
	}
}

PlayerConfigSerializer::PlayerConfigSerializer(PlayerConfig& config)
	: m_Config(config)
{
}

bool PlayerConfigSerializer::Serialize(const std::filesystem::path& filepath) const
{
	std::error_code error;
	if (!filepath.parent_path().empty())
	{
		std::filesystem::create_directories(filepath.parent_path(), error);
		if (error)
		{
			WHP_CORE_ERROR("[Whip Player] Could not create player config directory: {0}", error.message());
			return false;
		}
	}

	YAML::Emitter out;
	out << YAML::BeginMap;
	out << YAML::Key << "WhipPlayer" << YAML::Value << YAML::BeginMap;
	out << YAML::Key << "project" << YAML::Value << m_Config.m_ProjectPath.generic_string();
	out << YAML::Key << "product" << YAML::Value << m_Config.m_ProductName;
	out << YAML::Key << "version" << YAML::Value << m_Config.m_ProductVersion;
	out << YAML::Key << "company" << YAML::Value << m_Config.m_CompanyName;
	out << YAML::Key << "icon" << YAML::Value << m_Config.m_ProductIconPath.generic_string();
	out << YAML::Key << "log_file" << YAML::Value << m_Config.m_LogFilePath.generic_string();
	out << YAML::Key << "title" << YAML::Value << m_Config.m_WindowTitle;
	out << YAML::Key << "width" << YAML::Value << m_Config.m_WindowWidth;
	out << YAML::Key << "height" << YAML::Value << m_Config.m_WindowHeight;
	out << YAML::Key << "fullscreen" << YAML::Value << m_Config.m_Fullscreen;
	out << YAML::EndMap;
	out << YAML::EndMap;

	std::ofstream stream(filepath, std::ios::binary | std::ios::trunc);
	if (!stream)
	{
		WHP_CORE_ERROR("[Whip Player] Could not write player config: {0}", filepath.string());
		return false;
	}

	stream << out.c_str();
	return true;
}

bool PlayerConfigSerializer::Deserialize(const std::filesystem::path& filepath) const
{
	if (filepath.empty())
		return false;

	std::error_code error;
	if (!std::filesystem::exists(filepath, error))
		return false;

	YAML::Node data;
	try
	{
		data = YAML::LoadFile(filepath.string());
	}
	catch (const YAML::Exception& exception)
	{
		WHP_CORE_WARN("[Whip Player] Could not read player config '{0}': {1}", filepath.string(), exception.what());
		return false;
	}

	YAML::Node playerNode = FindMapValue(data, "WhipPlayer");
	if (!HasNode(playerNode))
		playerNode = FindMapValue(data, "whip_player");
	if (!HasNode(playerNode))
		return false;

	if (YAML::Node value = ReadValue(playerNode, "project", "Project"); HasNode(value))
		m_Config.m_ProjectPath = value.as<std::string>("");
	if (YAML::Node value = ReadValue(playerNode, "product", "Product"); HasNode(value))
		m_Config.m_ProductName = value.as<std::string>(m_Config.m_ProductName);
	if (YAML::Node value = ReadValue(playerNode, "version", "Version"); HasNode(value))
		m_Config.m_ProductVersion = value.as<std::string>(m_Config.m_ProductVersion);
	if (YAML::Node value = ReadValue(playerNode, "company", "Company"); HasNode(value))
		m_Config.m_CompanyName = value.as<std::string>(m_Config.m_CompanyName);
	if (YAML::Node value = ReadValue(playerNode, "icon", "Icon"); HasNode(value))
		m_Config.m_ProductIconPath = value.as<std::string>("");
	if (YAML::Node value = ReadValue(playerNode, "log_file", "LogFile"); HasNode(value))
		m_Config.m_LogFilePath = value.as<std::string>("");
	if (YAML::Node value = ReadValue(playerNode, "title", "Title"); HasNode(value))
		m_Config.m_WindowTitle = value.as<std::string>(m_Config.m_WindowTitle);
	if (YAML::Node value = ReadValue(playerNode, "width", "Width"); HasNode(value))
		m_Config.m_WindowWidth = std::max<uint32_t>(320, value.as<uint32_t>(m_Config.m_WindowWidth));
	if (YAML::Node value = ReadValue(playerNode, "height", "Height"); HasNode(value))
		m_Config.m_WindowHeight = std::max<uint32_t>(180, value.as<uint32_t>(m_Config.m_WindowHeight));
	if (YAML::Node value = ReadValue(playerNode, "fullscreen", "Fullscreen"); HasNode(value))
		m_Config.m_Fullscreen = value.as<bool>(m_Config.m_Fullscreen);

	return !m_Config.m_ProjectPath.empty();
}

std::filesystem::path PlayerConfigSerializer::GetDefaultConfigPath(const std::filesystem::path& rootDirectory)
{
	return rootDirectory / "WhipPlayer.yaml";
}

_WHIP_END
