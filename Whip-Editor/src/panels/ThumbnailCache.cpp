#include <WhipPch.h>
#include <Whip-Editor/panels/ThumbnailCache.h>

#include <Whip/Asset/TextureImporter.h>

#include <chrono>
#include <algorithm>
#include <cctype>

_WHIP_START

ThumbnailCache::ThumbnailCache(Ref<Project> project) : m_Project(project)
{
	// todo (move to cache dir)
	m_ThumbnailCachePath = m_Project->GetAssetDirectory() / "Thumbnail.cache";
}

Ref<Texture2D> ThumbnailCache::GetOrCreateThumbnail(const std::filesystem::path& path)
{
	auto absolutePath = m_Project->GetAssetAbsolutePath(path);
	const auto now = std::chrono::steady_clock::now();

	if (auto it = m_CachedImages.find(path); it != m_CachedImages.end())
	{
		ThumbnailImage& cachedImage = it->second;
		if (cachedImage.m_Image && now < cachedImage.m_NextValidationTime)
			return cachedImage.m_Image;
	}

	std::error_code error;
	std::filesystem::file_time_type lastWriteTime = std::filesystem::last_write_time(absolutePath, error);
	if (error)
		return nullptr;
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

	std::string extension = path.extension().string();
	std::ranges::transform(extension, extension.begin(),
		[](unsigned char character) { return static_cast<char>(std::tolower(character)); });
	if (extension != ".png" && extension != ".jpg" && extension != ".jpeg")
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
