#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Project/Project.h>
#include <Whip/Render/Texture.h>

#include <chrono>
#include <filesystem>
#include <map>

_WHIP_START

struct ThumbnailImage
{
	uint64_t m_Timestamp = 0;
	std::chrono::steady_clock::time_point m_NextValidationTime = {};
	Ref<Texture2D> m_Image;
};

class ThumbnailCache
{
public:
	ThumbnailCache(Ref<Project> project);

	Ref<Texture2D> GetOrCreateThumbnail(const std::filesystem::path& path);
private:
	Ref<Project> m_Project;

	std::map<std::filesystem::path, ThumbnailImage> m_CachedImages;

	// temp replace w whip::serialization
	std::filesystem::path m_ThumbnailCachePath;
};

_WHIP_END
