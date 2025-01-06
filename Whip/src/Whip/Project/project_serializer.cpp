#include "whippch.h"
#include "project_serializer.h"

#include <fstream>
#ifndef YAML_CPP_STATIC_DEFINE
#define YAML_CPP_STATIC_DEFINE
#endif // !YAML_CPP_STATIC_DEFINE
#include <yaml-cpp/yaml.h>

_WHIP_START

project_serializer::project_serializer(ref<project> proj) : m_project(proj) {}

bool project_serializer::serialize(const std::filesystem::path& filepath)
{
	const auto& config = m_project->get_config();

	YAML::Emitter out;
	{
		out << YAML::BeginMap; // Root
		out << YAML::Key << "project" << YAML::Value;
		{
			out << YAML::BeginMap;
			out << YAML::Key << "name" << YAML::Value << config.name;
			out << YAML::Key << "start_scene" << YAML::Value << (uint64_t)config.start_scene;
			out << YAML::Key << "asset_directory" << YAML::Value << config.asset_directory.string();
			out << YAML::Key << "asset_registry_path" << YAML::Value << config.asset_registry_path.string();
			out << YAML::Key << "script_module_path" << YAML::Value << config.script_module_path.string();
			out << YAML::EndMap;
		}
		out << YAML::EndMap; // Root
	}

	std::ofstream fout(filepath);
	fout << out.c_str();

	return true;
}

bool project_serializer::deserialize(const std::filesystem::path& filepath)
{
	auto& config = m_project->get_config();

	YAML::Node data;
	try
	{
		data = YAML::LoadFile(filepath.string());
	}
	catch (YAML::ParserException e)
	{
		WHP_CORE_ERROR("Failed to load project file '{0}' -> {1}", filepath.string(), e.what());
		return false;
	}

	auto project_node = data["project"];
	if (!project_node)
		return false;

	config.name = project_node["name"].as<std::string>();
	config.start_scene = project_node["start_scene"].as<uint64_t>();
	config.asset_directory = project_node["asset_directory"].as<std::string>();
	if(project_node["asset_registry_path"])
		config.asset_registry_path = project_node["asset_registry_path"].as<std::string>();
	config.script_module_path = project_node["script_module_path"].as<std::string>();
	return true;
}

_WHIP_END
