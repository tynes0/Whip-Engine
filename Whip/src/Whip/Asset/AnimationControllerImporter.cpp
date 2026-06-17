#include "WhipPch.h"
#include "Whip/Asset/AnimationControllerImporter.h"
#include "Whip/Project/Project.h"

_WHIP_START

Ref<AnimationController> AnimationControllerImporter::ImportAnimationController(AssetHandle handle, const AssetMetadata& metadata)
{
	return LoadAnimationController(Project::GetActiveAssetDirectory() / metadata.m_Filepath, handle);
}

Ref<AnimationController> AnimationControllerImporter::LoadAnimationController(const std::filesystem::path& path, AssetHandle handle)
{
	Ref<AnimationController> controller = MakeRef<AnimationController>(handle);
	if (!controller->Deserialize(path))
	{
		WHP_CORE_ERROR("[Animation Controller Importer] Failed to load controller from path: {0}", path.string());
		return nullptr;
	}
	return controller;
}

_WHIP_END
