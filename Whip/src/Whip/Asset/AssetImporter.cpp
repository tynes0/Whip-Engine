#include "WhipPch.h"
#include "Whip/Asset/AssetImporter.h"
#include "Whip/Asset/TextureImporter.h"
#include "Whip/Asset/SceneImporter.h"
#include "Whip/Asset/FontImporter.h"
#include "Whip/Asset/AudioImporter.h"
#include "Whip/Asset/AnimationImporter.h"
#include "Whip/Asset/AnimationControllerImporter.h"
#include "Whip/Asset/EntityTemplateImporter.h"

#include <map>
#include <functional>

_WHIP_START


namespace
{
	using AssetImportFunction = std::function<Ref<Asset>(AssetHandle, const AssetMetadata&)>;

	std::map<AssetType, AssetImportFunction> s_AssetImportFunctions =
	{
			{ AssetType::Texture2D, TextureImporter::ImportTexture2D },
			{ AssetType::Scene, SceneImporter::ImportScene },
			{ AssetType::Audio, AudioImporter::ImportAudio },
			{ AssetType::Font, FontImporter::ImportFont },
			{ AssetType::Animation, AnimationImporter::ImportAnimation },
			{ AssetType::AnimationController, AnimationControllerImporter::ImportAnimationController },
			{ AssetType::Entity, EntityTemplateImporter::ImportEntityTemplate }
	};
}



Ref<Asset> AssetImporter::ImportAsset(AssetHandle handle, const AssetMetadata& metadata)
{
	if (!s_AssetImportFunctions.contains(metadata.m_Type))
	{
		WHP_CORE_ERROR("[Asset Manager] No importer available for Asset type {}", frenum::to_string(metadata.m_Type));
		return nullptr;
	}
	return s_AssetImportFunctions.at(metadata.m_Type)(handle, metadata);
}

_WHIP_END
