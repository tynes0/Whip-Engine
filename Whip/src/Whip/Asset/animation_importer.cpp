#include "whippch.h"
#include "animation_importer.h"
#include <Whip/Project/project.h>
#include <Whip/Animation/animation_manager.h>

_WHIP_START

ref<animation2D> animation_importer::import_animation(asset_handle handle, const asset_metadata& metadata)
{
	return load_animation(project::get_active_asset_directory() / metadata.filepath, handle);
}

ref<animation2D> animation_importer::load_animation(const std::filesystem::path& path, asset_handle handle)
{
	auto animation = make_ref<animation2D>(handle);
	if (!animation->deserialize(path))
	{
		WHP_CORE_ERROR("[Animation Importer] Failed to load animation from path: {0}", path.string());
		return nullptr;
	}
	animation_manager::get().add_animation(animation);
	return animation;
}
_WHIP_END
