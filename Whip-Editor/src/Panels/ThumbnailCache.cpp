#include <WhipPch.h>
#include <Whip-Editor/Panels/ThumbnailCache.h>

#include <Whip/Asset/TextureImporter.h>

#include <chrono>
#include <algorithm>

_WHIP_START

ThumbnailCache::ThumbnailCache(Ref<Project> project) : m_Project(std::move(project))
{
	// todo (move to cache dir)
	m_ThumbnailCachePath = m_Project->GetAssetDirectory() / "Thumbnail.cache";
}

namespace
{
	bool IsSupportedThumbnailFile(const std::filesystem::path& path)
	{
		std::string extension = path.extension().string();
		std::ranges::transform(extension, extension.begin(),
			[](unsigned char character) { return static_cast<char>(std::tolower(character)); });
		return extension == ".png" || extension == ".jpg" || extension == ".jpeg";
	}
}

Ref<Texture2D> ThumbnailCache::GetThumbnail(const std::filesystem::path& path)
{
	WHP_PROFILE_FUNCTION();
	const auto now = std::chrono::steady_clock::now();

	if (auto it = m_CachedImages.find(path); it != m_CachedImages.end())
	{
		ThumbnailImage& cachedImage = it->second;
		if (cachedImage.m_Image && now < cachedImage.m_NextValidationTime)
			return cachedImage.m_Image;
		if (cachedImage.m_Image)
		{
			QueueThumbnail(path);
			return cachedImage.m_Image;
		}
	}

	QueueThumbnail(path);
	return nullptr;
}

void ThumbnailCache::ProcessPendingThumbnails(uint32_t maxCount)
{
	WHP_PROFILE_FUNCTION();
	const auto now = std::chrono::steady_clock::now();
	while (maxCount > 0 && !m_PendingThumbnailPaths.empty())
	{
		std::filesystem::path path = std::move(m_PendingThumbnailPaths.front());
		m_PendingThumbnailPaths.pop_front();
		m_PendingThumbnailSet.erase(path);
		LoadOrRefreshThumbnail(path, now);
		--maxCount;
	}
}

void ThumbnailCache::QueueThumbnail(const std::filesystem::path& path)
{
	if (path.empty() || !IsSupportedThumbnailFile(path) || m_PendingThumbnailSet.contains(path))
		return;

	m_PendingThumbnailSet.insert(path);
	m_PendingThumbnailPaths.push_back(path);
}

Ref<Texture2D> ThumbnailCache::LoadOrRefreshThumbnail(const std::filesystem::path& path, std::chrono::steady_clock::time_point now)
{
	WHP_PROFILE_FUNCTION();
	auto absolutePath = m_Project->GetAssetAbsolutePath(path);
	std::error_code error;
	std::filesystem::file_time_type lastWriteTime = std::filesystem::last_write_time(absolutePath, error);
	if (error)
	{
		m_CachedImages.erase(path);
		return nullptr;
	}
	uint64_t timestamp = static_cast<uint64_t>(lastWriteTime.time_since_epoch().count());

	if (auto it = m_CachedImages.find(path); it != m_CachedImages.end())
	{
		ThumbnailImage& cachedImage = it->second;
		if (cachedImage.m_Image && cachedImage.m_Timestamp == timestamp)
		{
			cachedImage.m_NextValidationTime = now + std::chrono::milliseconds(750);
			return cachedImage.m_Image;
		}
	}

	if (!IsSupportedThumbnailFile(path))
		return nullptr;

	Ref<Texture2D> texture = TextureImporter::LoadTexture2D(absolutePath);
	if (!texture)
		return nullptr;

	auto& cachedImage = m_CachedImages[path];
	cachedImage.m_Timestamp = timestamp;
	cachedImage.m_NextValidationTime = now + std::chrono::milliseconds(750);
	cachedImage.m_Image = texture;
	return cachedImage.m_Image;
}

_WHIP_END
