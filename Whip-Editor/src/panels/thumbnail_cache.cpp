#include <whippch.h>
#include "thumbnail_cache.h"

#include <Whip/Asset/texture_importer.h>

#include <chrono>

_WHIP_START

thumbnail_cache::thumbnail_cache(ref<project> proj) : m_project(proj)
{
	// todo (move to cache dir)
	m_thumbnail_cache_path = m_project->get_asset_directory() / "Thumbnail.cache";
}

ref<texture2D> thumbnail_cache::get_or_create_thumbnail(const std::filesystem::path& path)
{
	auto absolute_path = m_project->get_asset_absolute_path(path);
	std::filesystem::file_time_type last_write_time = std::filesystem::last_write_time(absolute_path);
	uint64_t timestamp = std::chrono::duration_cast<std::chrono::seconds>(last_write_time.time_since_epoch()).count();

	if (m_cached_images.find(path) != m_cached_images.end())
	{
		auto& cached_image = m_cached_images.at(path);
		if (cached_image.timestamp == timestamp)
			return cached_image.image;
	}

	// TODO: PNGs for now
	if (path.extension() != ".png" && path.extension() != ".jpg" && path.extension() != ".jpeg")
		return nullptr;

	ref<texture2D> texture = texture_importer::load_texture2D(absolute_path);
	if (!texture)
		return nullptr;

	auto& cached_image = m_cached_images[path];
	cached_image.timestamp = timestamp;
	cached_image.image = texture;
	return cached_image.image;
}

_WHIP_END
