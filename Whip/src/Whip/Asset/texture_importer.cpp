#include "whippch.h"
#include "texture_importer.h"
#include "asset_manager.h"

#include <Whip/Project/project.h>

#include <stb_image.h>

_WHIP_START

ref<texture2D> texture_importer::import_texture2D(asset_handle handle, const asset_metadata& metadata)
{
	ref<texture2D> result = load_texture2D(project::get_active_asset_directory() / metadata.filepath);
	result->handle = handle;
	return result;
}

ref<texture2D> texture_importer::load_texture2D(const std::filesystem::path& path, flip_direction_t direction)
{
	WHP_PROFILE_FUNCTION();
	int width, height, channels;
	raw_buffer data;

	{
		WHP_PROFILE_SCOPE("stbi_load - texture_importer::import_texture2D");
		std::string pathStr = path.string();
		data.data = stbi_load(pathStr.c_str(), &width, &height, &channels, STBI_rgb_alpha);
		channels = 4;
	}

	if (data.data == nullptr)
	{
		WHP_CORE_ERROR("[Asset Manager] Could not load texture from filepath: {}", path.string());
		return nullptr;
	}

	// TODO: think about this
	data.size = static_cast<uint64_t>(width * height * channels);

	texture_specification spec;
	spec.width = width;
	spec.height = height;
	switch (channels)
	{
	case 3:
		spec.format = image_format::RGB8;
		break;
	case 4:
		spec.format = image_format::RGBA8;
		break;
	}

	if(!(direction & flip_direction_vertical))
		texture2D::flip_texture_buffer(data, width, height, channels, flip_direction_vertical); // base case
	if (direction & flip_direction_horizontal)
		texture2D::flip_texture_buffer(data, width, height, channels, flip_direction_horizontal);

	ref<texture2D> texture = texture2D::create(spec, data);
	data.release();
	return texture;
}

_WHIP_END
