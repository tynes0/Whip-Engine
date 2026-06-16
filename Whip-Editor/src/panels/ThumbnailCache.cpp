#include <WhipPch.h>
#include "ThumbnailCache.h"

#include <Whip/Asset/TextureImporter.h>

#include <chrono>

_WHIP_START

ThumbnailCache::ThumbnailCache(Ref<Project> project) : m_Project(project)
{
	// todo (move to cache dir)
	m_ThumbnailCachePath = m_Project->GetAssetDirectory() / "Thumbnail.cache";
}

Ref<Texture2D> ThumbnailCache::GetOrCreateThumbnail(const std::filesystem::path& path)
{
	auto absolutePath = m_Project->GetAssetAbsolutePath(path);
	std::filesystem::file_time_type lastWriteTime = std::filesystem::last_write_time(absolutePath);
	uint64_t timestamp = std::chrono::duration_cast<std::chrono::seconds>(lastWriteTime.time_since_epoch()).count();

	if (m_CachedImages.find(path) != m_CachedImages.end())
	{
		auto& cachedImage = m_CachedImages.at(path);
		if (cachedImage.m_Timestamp == timestamp)
			return cachedImage.m_Image;
	}

	// TODO: PNGs for now
	if (path.extension() != ".png" && path.extension() != ".jpg" && path.extension() != ".jpeg")
		return nullptr;

	Ref<Texture2D> texture = TextureImporter::LoadTexture2D(absolutePath);
	if (!texture)
		return nullptr;

	auto& cachedImage = m_CachedImages[path];
	cachedImage.m_Timestamp = timestamp;
	cachedImage.m_Image = texture;
	return cachedImage.m_Image;
}

_WHIP_END
