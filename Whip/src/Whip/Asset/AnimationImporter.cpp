#include "WhipPch.h"
#include <Whip/Asset/AnimationImporter.h>
#include <Whip/Project/Project.h>
#include <Whip/Animation/AnimationManager.h>

_WHIP_START

Ref<Animation2D> AnimationImporter::ImportAnimation(AssetHandle handle, const AssetMetadata& metadata)
{
	return LoadAnimation(Project::GetActiveAssetDirectory() / metadata.m_Filepath, handle);
}

Ref<Animation2D> AnimationImporter::LoadAnimation(const std::filesystem::path& path, AssetHandle handle)
{
	auto animation = MakeRef<Animation2D>(handle);
	if (!animation->Deserialize(path))
	{
		WHP_CORE_ERROR("[Animation Importer] Failed to load animation from path: {0}", path.string());
		return nullptr;
	}
	AnimationManager::Get().AddAnimation(animation);
	return animation;
}
_WHIP_END
