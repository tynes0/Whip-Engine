#include "WhipPch.h"
#include <Whip/Project/ProjectSerializer.h>

#include <fstream>
#include <string>
#include <algorithm>
#ifndef YAML_CPP_STATIC_DEFINE
#define YAML_CPP_STATIC_DEFINE
#endif // !YAML_CPP_STATIC_DEFINE
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

	YAML::Node ReadProjectNode(const YAML::Node& data)
	{
		YAML::Node projectNode = FindMapValue(data, "Project");
		if (!HasNode(projectNode))
			projectNode = FindMapValue(data, "project");
		return projectNode;
	}

	YAML::Node ReadProjectValue(const YAML::Node& projectNode, const char* legacyKey, const char* pascalKey)
	{
		YAML::Node value = FindMapValue(projectNode, legacyKey);
		if (!HasNode(value))
			value = FindMapValue(projectNode, pascalKey);
		return value;
	}

	std::string ReadProjectString(const YAML::Node& projectNode, const char* legacyKey, const char* pascalKey, const std::string& defaultValue = {})
	{
		YAML::Node value = ReadProjectValue(projectNode, legacyKey, pascalKey);
		return HasNode(value) ? value.as<std::string>(defaultValue) : defaultValue;
	}

	bool ReadProjectBool(const YAML::Node& projectNode, const char* legacyKey, const char* pascalKey, bool defaultValue)
	{
		YAML::Node value = ReadProjectValue(projectNode, legacyKey, pascalKey);
		return HasNode(value) ? value.as<bool>(defaultValue) : defaultValue;
	}

	int ReadProjectInt(const YAML::Node& projectNode, const char* legacyKey, const char* pascalKey, int defaultValue)
	{
		YAML::Node value = ReadProjectValue(projectNode, legacyKey, pascalKey);
		return HasNode(value) ? value.as<int>(defaultValue) : defaultValue;
	}

	AssetHandle ReadProjectStartScene(const YAML::Node& projectNode, const std::filesystem::path& filepath)
	{
		YAML::Node startScene = ReadProjectValue(projectNode, "start_scene", "StartScene");
		if (!HasNode(startScene))
			return 0;

		try
		{
			return startScene.as<uint64_t>();
		}
		catch (const YAML::Exception&)
		{
			const std::string startSceneValue = startScene.as<std::string>("");
			if (startSceneValue.empty())
				return 0;

			try
			{
				size_t processed = 0;
				const uint64_t parsedHandle = std::stoull(startSceneValue, &processed);
				if (processed == startSceneValue.size())
					return parsedHandle;
			}
			catch (const std::exception&)
			{
			}

			WHP_CORE_WARN("Ignoring legacy Project start scene '{0}' in '{1}'.", startSceneValue, filepath.string());
			return 0;
		}
	}
}

ProjectSerializer::ProjectSerializer(Ref<Project> project) : m_Project(project) {}

bool ProjectSerializer::Serialize(const std::filesystem::path& filepath)
{
	const auto& config = m_Project->GetConfig();

	YAML::Emitter out;
	{
		out << YAML::BeginMap; // Root
		out << YAML::Key << "Project" << YAML::Value;
		{
			out << YAML::BeginMap;
			out << YAML::Key << "name" << YAML::Value << config.m_Name;
			out << YAML::Key << "start_scene" << YAML::Value << (uint64_t)config.m_StartScene;
			out << YAML::Key << "asset_directory" << YAML::Value << config.m_AssetDirectory.string();
			out << YAML::Key << "asset_registry_path" << YAML::Value << config.m_AssetRegistryPath.string();
			out << YAML::Key << "script_module_path" << YAML::Value << config.m_ScriptModulePath.string();
			out << YAML::Key << "script_debugger" << YAML::Value;
			{
				out << YAML::BeginMap;
				out << YAML::Key << "enabled" << YAML::Value << config.m_EnableScriptDebugging;
				out << YAML::Key << "host" << YAML::Value << config.m_ScriptDebuggerHost;
				out << YAML::Key << "port" << YAML::Value << config.m_ScriptDebuggerPort;
				out << YAML::Key << "suspend_on_start" << YAML::Value << config.m_ScriptDebuggerSuspendOnStart;
				out << YAML::Key << "log_file" << YAML::Value << config.m_ScriptDebuggerLogFile;
				out << YAML::EndMap;
			}
			out << YAML::EndMap;
		}
		out << YAML::EndMap; // Root
	}

	std::ofstream fout(filepath);
	if (!fout)
	{
		WHP_CORE_ERROR("Failed to save Project file '{0}'", filepath.string());
		return false;
	}
	fout << out.c_str();

	return true;
}

bool ProjectSerializer::Deserialize(const std::filesystem::path& filepath)
{
	auto& config = m_Project->GetConfig();

	YAML::Node data;
	try
	{
		data = YAML::LoadFile(filepath.string());
	}
	catch (const YAML::Exception& e)
	{
		WHP_CORE_ERROR("Failed to load Project file '{0}' -> {1}", filepath.string(), e.what());
		return false;
	}

	try
	{
		auto projectNode = ReadProjectNode(data);
		if (!HasNode(projectNode))
		{
			WHP_CORE_ERROR("Failed to load Project file '{0}' -> missing Project root node.", filepath.string());
			return false;
		}

		config.m_Name = ReadProjectString(projectNode, "name", "Name", config.m_Name);
		config.m_StartScene = ReadProjectStartScene(projectNode, filepath);
		config.m_AssetDirectory = ReadProjectString(projectNode, "asset_directory", "AssetDirectory", config.m_AssetDirectory.string());
		config.m_AssetRegistryPath = ReadProjectString(projectNode, "asset_registry_path", "AssetRegistryPath", FileExtensions::AssetRegistryFilename);
		if (config.m_AssetRegistryPath.empty())
			config.m_AssetRegistryPath = FileExtensions::AssetRegistryFilename;
		config.m_ScriptModulePath = ReadProjectString(projectNode, "script_module_path", "ScriptModulePath");

		YAML::Node debuggerNode = ReadProjectValue(projectNode, "script_debugger", "ScriptDebugger");
		if (HasNode(debuggerNode))
		{
			config.m_EnableScriptDebugging = ReadProjectBool(debuggerNode, "enabled", "Enabled", config.m_EnableScriptDebugging);
			config.m_ScriptDebuggerHost = ReadProjectString(debuggerNode, "host", "Host", config.m_ScriptDebuggerHost);
			config.m_ScriptDebuggerPort = std::clamp(ReadProjectInt(debuggerNode, "port", "Port", config.m_ScriptDebuggerPort), 1, 65535);
			config.m_ScriptDebuggerSuspendOnStart = ReadProjectBool(debuggerNode, "suspend_on_start", "SuspendOnStart", config.m_ScriptDebuggerSuspendOnStart);
			config.m_ScriptDebuggerLogFile = ReadProjectString(debuggerNode, "log_file", "LogFile", config.m_ScriptDebuggerLogFile);
		}
		if (config.m_ScriptDebuggerHost.empty())
			config.m_ScriptDebuggerHost = "127.0.0.1";
		if (config.m_ScriptDebuggerLogFile.empty())
			config.m_ScriptDebuggerLogFile = "MonoDebugger.log";
	}
	catch (const YAML::Exception& e)
	{
		WHP_CORE_ERROR("Failed to load Project file '{0}' -> {1}", filepath.string(), e.what());
		return false;
	}
	catch (const std::exception& e)
	{
		WHP_CORE_ERROR("Failed to load Project file '{0}' -> {1}", filepath.string(), e.what());
		return false;
	}
	return true;
}

_WHIP_END
