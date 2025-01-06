#include "whippch.h"
#include "audio_importer.h"

#include <Whip/Project/project.h>

#include <Whip/Audio/audio_engine.h>

_WHIP_START

ref<audio_source> audio_importer::import_audio(asset_handle handle, const asset_metadata& metadata)
{
	return load_audio(project::get_active_asset_directory() / metadata.filepath, handle);
}

ref<audio_source> audio_importer::load_audio(const std::filesystem::path& path, asset_handle handle)
{
	return audio_engine::load_audio_source(path, handle);
}

_WHIP_END
