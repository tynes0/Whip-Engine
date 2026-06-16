#include "WhipPch.h"
#include <Whip/Asset/AudioImporter.h>

#include <Whip/Project/Project.h>

#include <Whip/Audio/AudioEngine.h>

_WHIP_START

Ref<AudioSource> AudioImporter::ImportAudio(AssetHandle handle, const AssetMetadata& metadata)
{
	return LoadAudio(Project::GetActiveAssetDirectory() / metadata.m_Filepath, handle);
}

Ref<AudioSource> AudioImporter::LoadAudio(const std::filesystem::path& path, AssetHandle handle)
{
	return AudioEngine::LoadAudioSource(path, handle);
}

_WHIP_END
