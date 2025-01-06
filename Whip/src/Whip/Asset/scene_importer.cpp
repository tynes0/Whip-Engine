#include "whippch.h"
#include "scene_importer.h"

#include <Whip/Project/project.h>
#include <Whip/Scene/scene_serializer.h>

_WHIP_START
	
ref<scene> scene_importer::import_scene(asset_handle handle, const asset_metadata& metadata)
{
	return load_scene(project::get_active_asset_directory() / metadata.filepath, handle);
}

ref<scene> scene_importer::load_scene(const std::filesystem::path& path, asset_handle handle)
{
	WHP_PROFILE_FUNCTION();
	ref<scene> scne = make_ref<scene>(handle);
	scene_serializer serializer(scne);
	serializer.deserialize(path);
	return scne;
}

void scene_importer::save_scene(ref<scene> scne, const std::filesystem::path& path)
{
	scene_serializer serializer(scne);
	serializer.serialize(project::get_active_asset_directory() / path);
}


_WHIP_END
