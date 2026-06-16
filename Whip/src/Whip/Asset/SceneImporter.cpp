#include "WhipPch.h"
#include <Whip/Asset/SceneImporter.h>

#include <Whip/Project/Project.h>
#include <Whip/Scene/SceneSerializer.h>

_WHIP_START
	
Ref<Scene> SceneImporter::ImportScene(AssetHandle handle, const AssetMetadata& metadata)
{
	return LoadScene(Project::GetActiveAssetDirectory() / metadata.m_Filepath, handle);
}

Ref<Scene> SceneImporter::LoadScene(const std::filesystem::path& path, AssetHandle handle)
{
	WHP_PROFILE_FUNCTION();
	Ref<Scene> scene = MakeRef<Scene>(handle);
	SceneSerializer serializer(scene);
	if (!serializer.Deserialize(path))
		return nullptr;
	return scene;
}

void SceneImporter::SaveScene(Ref<Scene> scene, const std::filesystem::path& path)
{
	std::filesystem::path scenePath = path;
	if (!scenePath.is_absolute())
		scenePath = Project::GetActiveAssetDirectory() / scenePath;

	SceneSerializer serializer(scene);
	serializer.Serialize(scenePath);
}


_WHIP_END
