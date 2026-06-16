#include "WhipPch.h"
#include <Whip/Asset/FontImporter.h>

#include <Whip/Project/Project.h>

_WHIP_START

Ref<Font> FontImporter::ImportFont(AssetHandle handle, const AssetMetadata& metadata)
{
	return LoadFont(Project::GetActiveAssetDirectory() / metadata.m_Filepath, handle);
}

Ref<Font> FontImporter::LoadFont(const std::filesystem::path& path, AssetHandle handle)
{
	return MakeRef<Font>(path, handle);
}

_WHIP_END
