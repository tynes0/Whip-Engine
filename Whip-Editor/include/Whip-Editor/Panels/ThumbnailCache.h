#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Project/Project.h>
#include <Whip/Render/Texture.h>

#include <chrono>
#include <deque>
#include <filesystem>
#include <map>
#include <set>

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

	Ref<Texture2D> GetThumbnail(const std::filesystem::path& path);
	void ProcessPendingThumbnails(uint32_t maxCount = 1);
private:
	void QueueThumbnail(const std::filesystem::path& path);
	Ref<Texture2D> LoadOrRefreshThumbnail(const std::filesystem::path& path, std::chrono::steady_clock::time_point now);

	Ref<Project> m_Project;

	std::map<std::filesystem::path, ThumbnailImage> m_CachedImages;
	std::deque<std::filesystem::path> m_PendingThumbnailPaths;
	std::set<std::filesystem::path> m_PendingThumbnailSet;

	// temp replace w whip::serialization
	std::filesystem::path m_ThumbnailCachePath;
};

_WHIP_END
