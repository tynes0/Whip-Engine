#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Project/project.h>
#include <Whip/Render/Texture.h>

_WHIP_START

struct thumbnail_image
{
	uint64_t timestamp = 0;
	ref<texture2D> image;
};

class thumbnail_cache
{
public:
	thumbnail_cache(ref<project> proj);

	ref<texture2D> get_or_create_thumbnail(const std::filesystem::path& path);
private:
	ref<project> m_project;

	std::map<std::filesystem::path, thumbnail_image> m_cached_images;

	// temp replace w whip::serialization
	std::filesystem::path m_thumbnail_cache_path;
};

_WHIP_END
