#include "WhipPch.h"
#include <Whip/Project/Project.h>
#include <Whip/Project/ProjectSerializer.h>

_WHIP_START

namespace
{
	bool MigrateAssetRegistryExtension(const Ref<Project>& projectIn)
	{
		if (!projectIn)
			return false;

		ProjectConfig& config = projectIn->GetConfig();
		if (!FileExtensions::ExtensionEquals(config.m_AssetRegistryPath, FileExtensions::AssetRegistryLegacy))
			return false;

		std::filesystem::path legacyRelativePath = config.m_AssetRegistryPath;
		std::filesystem::path modernRelativePath = legacyRelativePath;
		modernRelativePath.replace_extension(FileExtensions::AssetRegistry);

		const std::filesystem::path assetDirectory = projectIn->GetAssetDirectory();
		const std::filesystem::path legacyPath = assetDirectory / legacyRelativePath;
		const std::filesystem::path modernPath = assetDirectory / modernRelativePath;

		std::error_code error;
		if (std::filesystem::exists(modernPath, error))
		{
			config.m_AssetRegistryPath = modernRelativePath;
			return true;
		}

		if (std::filesystem::exists(legacyPath, error))
		{
			std::filesystem::rename(legacyPath, modernPath, error);
			if (error)
			{
				error.clear();
				std::filesystem::copy_file(legacyPath, modernPath, std::filesystem::copy_options::overwrite_existing, error);
				if (error)
					return false;
			}
		}

		config.m_AssetRegistryPath = modernRelativePath;
		return true;
	}
}

std::filesystem::path Project::GetAssetAbsolutePath(const std::filesystem::path& path)
{
	return GetAssetDirectory() / path;
}

Ref<Project> Project::NewProject()
{
	s_ActiveProject = MakeRef<Project>();
	s_ActiveProject->m_Loaded = true;
	return s_ActiveProject;
}

Ref<Project> Project::LoadDetached(const std::filesystem::path& path)
{
	WHP_CORE_INFO("[Project] Load begin: {0}", path.string());
	Ref<Project> result = MakeRef<Project>();
	ProjectSerializer serializer(result);
	WHP_CORE_INFO("[Project] Deserializing Project file.");
	if (serializer.Deserialize(path))
	{
		WHP_CORE_INFO("[Project] Project file deserialized.");
		result->m_ProjectPath = path;
		result->m_ProjectDirectory = path.parent_path();

		WHP_CORE_INFO("[Project] Migrating Asset registry extension if needed.");
		if (MigrateAssetRegistryExtension(result))
		{
			ProjectSerializer migratedSerializer(result);
			migratedSerializer.Serialize(path);
		}

		WHP_CORE_INFO("[Project] Loading editor Asset registry.");
		std::shared_ptr<EditorAssetManager> editorAssetManager = MakeRefTagged<EditorAssetManager>(memory::MemoryTag::Asset);
		editorAssetManager->DeserializeAssetRegistry(result->GetAssetRegistryPath());
		result->m_EditorAssetManager = editorAssetManager;
		WHP_CORE_INFO("[Project] Editor Asset registry loaded.");

		std::shared_ptr<RuntimeAssetManager> runtimeAssetManager = MakeRefTagged<RuntimeAssetManager>(memory::MemoryTag::Asset);
		runtimeAssetManager->SetEditorAssetManager(editorAssetManager);
		result->m_RuntimeAssetManager = runtimeAssetManager;

		result->m_Loaded = true;
		WHP_CORE_INFO("[Project] Load complete.");
		return result;
	}
	WHP_CORE_WARN("[Project] Load failed during Project file deserialization: {0}", path.string());
	return nullptr;
}

Ref<Project> Project::Load(const std::filesystem::path& path)
{
	Ref<Project> project = LoadDetached(path);
	if (!project)
		return nullptr;

	s_ActiveProject = project;
	return s_ActiveProject;
}

bool Project::SaveActive()
{
	ProjectSerializer serializer(s_ActiveProject);
	if (serializer.Serialize(s_ActiveProject->m_ProjectPath))
		return true;
	return false;
}

bool Project::SaveActive(const std::filesystem::path& path)
{
	ProjectSerializer serializer(s_ActiveProject);
	if (serializer.Serialize(path))
	{
		s_ActiveProject->m_ProjectPath = path;
		s_ActiveProject->m_ProjectDirectory = path.parent_path();
		return true;
	}
	return false;
}

_WHIP_END
