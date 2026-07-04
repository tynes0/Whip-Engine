#include <Whip/Asset/TextureSlicer.h>

#include <Whip/Asset/AssetManager.h>
#include <Whip/Core/Memory/AllocatorRegistry.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <fstream>
#include <iterator>
#include <limits>
#include <span>
#include <string>
#include <vector>

_WHIP_START

namespace TextureSlicer
{
	namespace
	{
		struct Color
		{
			uint8_t m_R = 0;
			uint8_t m_G = 0;
			uint8_t m_B = 0;
			uint8_t m_A = 255;
		};

		struct Bounds
		{
			int32_t m_MinX = std::numeric_limits<int32_t>::max();
			int32_t m_MinY = std::numeric_limits<int32_t>::max();
			int32_t m_MaxX = std::numeric_limits<int32_t>::min();
			int32_t m_MaxY = std::numeric_limits<int32_t>::min();
			uint32_t m_Pixels = 0;
		};

		uint32_t BytesPerPixel(ImageFormat format)
		{
			switch (format)
			{
			case ImageFormat::Rgb8: return 3;
			case ImageFormat::Rgba8: return 4;
			default: return 0;
			}
		}

		Color ReadPixelTopLeft(const PixelBuffer& buffer, int32_t x, int32_t y)
		{
			if (x < 0 || y < 0 || x >= static_cast<int32_t>(buffer.m_Width) || y >= static_cast<int32_t>(buffer.m_Height) || buffer.m_Pixels.empty())
				return {};

			const uint32_t storageY = buffer.m_Height - 1 - static_cast<uint32_t>(y);
			const size_t index = (static_cast<size_t>(storageY) * buffer.m_Width + static_cast<uint32_t>(x)) * buffer.m_Channels;
			Color color;
			color.m_R = buffer.m_Pixels[index + 0];
			color.m_G = buffer.m_Pixels[index + 1];
			color.m_B = buffer.m_Pixels[index + 2];
			color.m_A = buffer.m_Channels == 4 ? buffer.m_Pixels[index + 3] : 255;
			return color;
		}

		int32_t ColorDistanceMaxChannel(const Color& left, const Color& right)
		{
			const int32_t dr = std::abs(static_cast<int32_t>(left.m_R) - static_cast<int32_t>(right.m_R));
			const int32_t dg = std::abs(static_cast<int32_t>(left.m_G) - static_cast<int32_t>(right.m_G));
			const int32_t db = std::abs(static_cast<int32_t>(left.m_B) - static_cast<int32_t>(right.m_B));
			return std::max({ dr, dg, db });
		}

		int32_t MinDistanceToBackgroundColors(const Color& color, const std::array<Color, 4>& backgroundColors)
		{
			int32_t result = std::numeric_limits<int32_t>::max();
			for (const Color& backgroundColor : backgroundColors)
				result = std::min(result, ColorDistanceMaxChannel(color, backgroundColor));
			return result;
		}

		bool IsNearAnyBackgroundColor(const Color& color, const std::array<Color, 4>& backgroundColors, int32_t tolerance)
		{
			return MinDistanceToBackgroundColors(color, backgroundColors) <= tolerance;
		}

		int32_t EstimateBackgroundTolerance(const PixelBuffer& buffer, const std::array<Color, 4>& backgroundColors, int32_t requestedTolerance, memory::Allocator& scratchAllocator)
		{
			const int32_t width = static_cast<int32_t>(buffer.m_Width);
			const int32_t height = static_cast<int32_t>(buffer.m_Height);
			if (width <= 0 || height <= 0)
				return requestedTolerance;

			memory::Vector<int32_t> borderDistances = memory::MakeVector<int32_t>(scratchAllocator, memory::MemoryTag::Temporary);
			borderDistances.reserve(static_cast<size_t>(width) * 2 + static_cast<size_t>(height) * 2);
			for (int32_t x = 0; x < width; ++x)
			{
				borderDistances.push_back(MinDistanceToBackgroundColors(ReadPixelTopLeft(buffer, x, 0), backgroundColors));
				borderDistances.push_back(MinDistanceToBackgroundColors(ReadPixelTopLeft(buffer, x, height - 1), backgroundColors));
			}
			for (int32_t y = 1; y + 1 < height; ++y)
			{
				borderDistances.push_back(MinDistanceToBackgroundColors(ReadPixelTopLeft(buffer, 0, y), backgroundColors));
				borderDistances.push_back(MinDistanceToBackgroundColors(ReadPixelTopLeft(buffer, width - 1, y), backgroundColors));
			}

			if (borderDistances.empty())
				return requestedTolerance;

			std::ranges::sort(borderDistances);
			const auto percentile = [&](float value)
				{
					const size_t index = std::min(borderDistances.size() - 1, static_cast<size_t>(value * static_cast<float>(borderDistances.size() - 1)));
					return borderDistances[index];
				};

			const int32_t p50 = percentile(0.50f);
			const int32_t p75 = percentile(0.75f);
			const int32_t p90 = percentile(0.90f);
			const int32_t robustBorderTolerance = std::min(p90, std::max(p75, p50) + 16);
			return std::clamp(std::max(requestedTolerance, robustBorderTolerance + 8), 0, 96);
		}

		void RemoveForegroundSpeckles(std::vector<uint8_t>& mask, int32_t width, int32_t height, memory::Allocator& scratchAllocator)
		{
			if (width <= 0 || height <= 0 || mask.size() != static_cast<size_t>(width) * height)
				return;

			memory::Vector<uint8_t> cleaned = memory::MakeVector<uint8_t>(scratchAllocator, memory::MemoryTag::Temporary);
			cleaned.assign(mask.begin(), mask.end());
			for (int32_t y = 0; y < height; ++y)
			{
				for (int32_t x = 0; x < width; ++x)
				{
					const size_t index = static_cast<size_t>(y) * width + x;
					if (!mask[index])
						continue;

					int32_t neighborCount = 0;
					for (int32_t oy = -1; oy <= 1; ++oy)
					{
						for (int32_t ox = -1; ox <= 1; ++ox)
						{
							if (ox == 0 && oy == 0)
								continue;
							const int32_t nx = x + ox;
							const int32_t ny = y + oy;
							if (nx < 0 || ny < 0 || nx >= width || ny >= height)
								continue;
							neighborCount += mask[static_cast<size_t>(ny) * width + nx] ? 1 : 0;
						}
					}

					if (neighborCount <= 1)
						cleaned[index] = 0;
				}
			}
			std::copy(cleaned.begin(), cleaned.end(), mask.begin());
		}

		bool BoundsOverlapWithGap(const Bounds& left, const Bounds& right, int32_t gap)
		{
			return left.m_MinX <= right.m_MaxX + gap && left.m_MaxX + gap >= right.m_MinX &&
				left.m_MinY <= right.m_MaxY + gap && left.m_MaxY + gap >= right.m_MinY;
		}

		bool ShouldMergeFragmentBounds(const Bounds& left, const Bounds& right, int32_t gap, uint32_t minPixels)
		{
			if (gap <= 0 || !BoundsOverlapWithGap(left, right, gap))
				return false;

			const Bounds& smaller = left.m_Pixels <= right.m_Pixels ? left : right;
			const Bounds& larger = left.m_Pixels <= right.m_Pixels ? right : left;
			const uint32_t smallFragmentLimit = std::max<uint32_t>(96, minPixels * 4);
			if (smaller.m_Pixels > smallFragmentLimit)
				return false;

			const int32_t centerX = (smaller.m_MinX + smaller.m_MaxX) / 2;
			const int32_t centerY = (smaller.m_MinY + smaller.m_MaxY) / 2;
			return centerX >= larger.m_MinX - gap && centerX <= larger.m_MaxX + gap &&
				centerY >= larger.m_MinY - gap && centerY <= larger.m_MaxY + gap;
		}

		void MergeBounds(Bounds& target, const Bounds& source)
		{
			target.m_MinX = std::min(target.m_MinX, source.m_MinX);
			target.m_MinY = std::min(target.m_MinY, source.m_MinY);
			target.m_MaxX = std::max(target.m_MaxX, source.m_MaxX);
			target.m_MaxY = std::max(target.m_MaxY, source.m_MaxY);
			target.m_Pixels += source.m_Pixels;
		}

		void WriteBigEndianU32(memory::Vector<uint8_t>& buffer, uint32_t value)
		{
			buffer.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
			buffer.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
			buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
			buffer.push_back(static_cast<uint8_t>(value & 0xFF));
		}

		uint32_t Crc32(const uint8_t* data, size_t size)
		{
			static uint32_t table[256]{};
			static bool initialized = false;
			if (!initialized)
			{
				for (uint32_t i = 0; i < 256; ++i)
				{
					uint32_t crc = i;
					for (int bit = 0; bit < 8; ++bit)
						crc = (crc & 1U) ? (0xEDB88320U ^ (crc >> 1U)) : (crc >> 1U);
					table[i] = crc;
				}
				initialized = true;
			}

			uint32_t crc = 0xFFFFFFFFU;
			for (size_t i = 0; i < size; ++i)
				crc = table[(crc ^ data[i]) & 0xFFU] ^ (crc >> 8U);
			return crc ^ 0xFFFFFFFFU;
		}

		uint32_t Adler32(std::span<const uint8_t> data)
		{
			constexpr uint32_t mod = 65521;
			uint32_t a = 1;
			uint32_t b = 0;
			for (uint8_t byte : data)
			{
				a = (a + byte) % mod;
				b = (b + a) % mod;
			}
			return (b << 16U) | a;
		}

		void WritePngChunk(memory::Vector<uint8_t>& png, const char type[4], std::span<const uint8_t> data)
		{
			WriteBigEndianU32(png, static_cast<uint32_t>(data.size()));
			const size_t crcStart = png.size();
			png.insert(png.end(), type, type + 4);
			png.insert(png.end(), data.begin(), data.end());
			const uint32_t crc = Crc32(png.data() + crcStart, png.size() - crcStart);
			WriteBigEndianU32(png, crc);
		}

		void ExtrudeTransparentRgb(memory::Vector<uint8_t>& rgbaPixels, uint32_t width, uint32_t height, uint32_t extrudePixels, memory::Allocator& scratchAllocator)
		{
			if (extrudePixels == 0 || width == 0 || height == 0 || rgbaPixels.size() != static_cast<size_t>(width) * height * 4)
				return;

			memory::Vector<uint8_t> filled = memory::MakeVectorWithSize<uint8_t>(scratchAllocator, static_cast<size_t>(width) * height, memory::MemoryTag::Temporary);
			for (uint32_t y = 0; y < height; ++y)
			{
				for (uint32_t x = 0; x < width; ++x)
				{
					const size_t pixelIndex = static_cast<size_t>(y) * width + x;
					filled[pixelIndex] = rgbaPixels[pixelIndex * 4 + 3] > 0 ? 1 : 0;
				}
			}

			static constexpr int32_t Directions4[4][2] =
			{
				{ 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 }
			};

			for (uint32_t pass = 0; pass < extrudePixels; ++pass)
			{
				memory::Vector<uint8_t> nextFilled = filled;
				memory::Vector<uint8_t> nextPixels = rgbaPixels;
				bool wrote = false;
				for (uint32_t y = 0; y < height; ++y)
				{
					for (uint32_t x = 0; x < width; ++x)
					{
						const size_t pixelIndex = static_cast<size_t>(y) * width + x;
						if (filled[pixelIndex])
							continue;

						for (const auto& direction : Directions4)
						{
							const int32_t nx = static_cast<int32_t>(x) + direction[0];
							const int32_t ny = static_cast<int32_t>(y) + direction[1];
							if (nx < 0 || ny < 0 || nx >= static_cast<int32_t>(width) || ny >= static_cast<int32_t>(height))
								continue;

							const size_t neighborIndex = static_cast<size_t>(ny) * width + static_cast<uint32_t>(nx);
							if (!filled[neighborIndex])
								continue;

							const size_t target = pixelIndex * 4;
							const size_t source = neighborIndex * 4;
							nextPixels[target + 0] = rgbaPixels[source + 0];
							nextPixels[target + 1] = rgbaPixels[source + 1];
							nextPixels[target + 2] = rgbaPixels[source + 2];
							nextPixels[target + 3] = 0;
							nextFilled[pixelIndex] = 1;
							wrote = true;
							break;
						}
					}
				}

				rgbaPixels.swap(nextPixels);
				filled.swap(nextFilled);
				if (!wrote)
					break;
			}
		}

		bool WritePngRgbaTopLeft(const std::filesystem::path& path, std::span<const uint8_t> rgbaPixels, uint32_t width, uint32_t height, std::string& error, memory::Allocator& scratchAllocator)
		{
			if (width == 0 || height == 0 || rgbaPixels.size() != static_cast<size_t>(width) * height * 4)
			{
				error = "Invalid slice PNG buffer.";
				return false;
			}

			memory::Vector<uint8_t> scanlines = memory::MakeVector<uint8_t>(scratchAllocator, memory::MemoryTag::Temporary);
			scanlines.reserve(static_cast<size_t>(height) * (static_cast<size_t>(width) * 4 + 1));
			for (uint32_t y = 0; y < height; ++y)
			{
				scanlines.push_back(0);
				const size_t rowStart = static_cast<size_t>(y) * width * 4;
				scanlines.insert(scanlines.end(), rgbaPixels.begin() + static_cast<std::ptrdiff_t>(rowStart), rgbaPixels.begin() + static_cast<std::ptrdiff_t>(rowStart + static_cast<size_t>(width) * 4));
			}

			memory::Vector<uint8_t> zlib = memory::MakeVector<uint8_t>(scratchAllocator, memory::MemoryTag::Temporary);
			zlib.reserve(scanlines.size() + scanlines.size() / 65535 * 5 + 16);
			zlib.push_back(0x78);
			zlib.push_back(0x01);

			size_t offset = 0;
			while (offset < scanlines.size())
			{
				const uint16_t blockSize = static_cast<uint16_t>(std::min<size_t>(65535, scanlines.size() - offset));
				const bool finalBlock = offset + blockSize >= scanlines.size();
				zlib.push_back(finalBlock ? 0x01 : 0x00);
				zlib.push_back(static_cast<uint8_t>(blockSize & 0xFF));
				zlib.push_back(static_cast<uint8_t>((blockSize >> 8) & 0xFF));
				const uint16_t inverse = static_cast<uint16_t>(~blockSize);
				zlib.push_back(static_cast<uint8_t>(inverse & 0xFF));
				zlib.push_back(static_cast<uint8_t>((inverse >> 8) & 0xFF));
				zlib.insert(zlib.end(), scanlines.begin() + static_cast<std::ptrdiff_t>(offset), scanlines.begin() + static_cast<std::ptrdiff_t>(offset + blockSize));
				offset += blockSize;
			}

			WriteBigEndianU32(zlib, Adler32(std::span<const uint8_t>(scanlines.data(), scanlines.size())));

			memory::Vector<uint8_t> png = memory::MakeVector<uint8_t>(scratchAllocator, memory::MemoryTag::Temporary);
			png.insert(png.end(), { 137, 80, 78, 71, 13, 10, 26, 10 });
			memory::Vector<uint8_t> ihdr = memory::MakeVector<uint8_t>(scratchAllocator, memory::MemoryTag::Temporary);
			ihdr.reserve(13);
			WriteBigEndianU32(ihdr, width);
			WriteBigEndianU32(ihdr, height);
			ihdr.push_back(8);
			ihdr.push_back(6);
			ihdr.push_back(0);
			ihdr.push_back(0);
			ihdr.push_back(0);
			WritePngChunk(png, "IHDR", std::span<const uint8_t>(ihdr.data(), ihdr.size()));
			WritePngChunk(png, "IDAT", std::span<const uint8_t>(zlib.data(), zlib.size()));
			WritePngChunk(png, "IEND", std::span<const uint8_t>());

			std::ofstream output(path, std::ios::binary | std::ios::trunc);
			if (!output)
			{
				error = "Could not open slice PNG for writing.";
				return false;
			}

			output.write(reinterpret_cast<const char*>(png.data()), static_cast<std::streamsize>(png.size()));
			if (!output)
			{
				error = "Could not finish writing slice PNG.";
				return false;
			}
			return true;
		}
	}

	bool LoadTexturePixels(AssetHandle handle, PixelBuffer& buffer, std::string& error)
	{
		buffer = {};
		if (!AssetManager::IsAssetHandleValid(handle) || AssetManager::GetAssetType(handle) != AssetType::Texture2D)
		{
			error = "Asset is not a texture.";
			return false;
		}

		Ref<Texture2D> texture = AssetManager::GetAsset<Texture2D>(handle);
		if (!texture || !texture->IsLoaded())
		{
			error = "Texture is not loaded.";
			return false;
		}

		const TextureSpecification& specification = texture->GetSpecification();
		const uint32_t channels = BytesPerPixel(specification.m_Format);
		if (channels == 0)
		{
			error = "Texture format is not supported by the slicer.";
			return false;
		}

		RawBuffer data = texture->GetData();
		if (!data)
		{
			error = "Could not read texture pixels.";
			return false;
		}

		buffer.m_Width = specification.m_Width;
		buffer.m_Height = specification.m_Height;
		buffer.m_Channels = channels;
		buffer.m_Format = specification.m_Format;
		buffer.m_Pixels.assign(data.m_Data, data.m_Data + data.m_Size);
		data.Release();

		const uint64_t expectedSize = static_cast<uint64_t>(buffer.m_Width) * buffer.m_Height * buffer.m_Channels;
		if (buffer.m_Pixels.size() != expectedSize)
		{
			buffer = {};
			error = "Texture pixel buffer size is invalid.";
			return false;
		}

		return true;
	}

	AutoSliceResult DetectSprites(const PixelBuffer& buffer, const std::string& namePrefix, const AutoSliceOptions& options)
	{
		AutoSliceResult result;
		if (buffer.m_Width == 0 || buffer.m_Height == 0 || buffer.m_Pixels.empty() || (buffer.m_Channels != 3 && buffer.m_Channels != 4))
		{
			result.m_Error = "Texture pixels are not readable.";
			return result;
		}

		const int32_t width = static_cast<int32_t>(buffer.m_Width);
		const int32_t height = static_cast<int32_t>(buffer.m_Height);
		const size_t pixelCount = static_cast<size_t>(width) * height;
		memory::ArenaAllocator scratchArena(memory::Megabytes(8), &memory::GetAllocator(memory::MemoryTag::Asset), "TextureSlicerDetectArena");
		memory::Vector<uint8_t> candidateBackground = memory::MakeVectorWithSize<uint8_t>(scratchArena, pixelCount, memory::MemoryTag::Temporary);
		result.m_ForegroundMask.assign(pixelCount, 0);

		bool hasTransparentPixels = false;
		if (buffer.m_Channels == 4)
		{
			for (int32_t y = 0; y < height && !hasTransparentPixels; ++y)
			{
				for (int32_t x = 0; x < width; ++x)
				{
					if (ReadPixelTopLeft(buffer, x, y).m_A <= options.m_AlphaThreshold)
					{
						hasTransparentPixels = true;
						break;
					}
				}
			}
		}
		result.m_UsedAlpha = hasTransparentPixels;

		if (hasTransparentPixels)
		{
			for (int32_t y = 0; y < height; ++y)
			{
				for (int32_t x = 0; x < width; ++x)
				{
					const size_t index = static_cast<size_t>(y) * width + x;
					result.m_ForegroundMask[index] = ReadPixelTopLeft(buffer, x, y).m_A > options.m_AlphaThreshold ? 1 : 0;
				}
			}
		}
		else
		{
			const std::array<Color, 4> backgroundColors =
			{
				ReadPixelTopLeft(buffer, 0, 0),
				ReadPixelTopLeft(buffer, width - 1, 0),
				ReadPixelTopLeft(buffer, 0, height - 1),
				ReadPixelTopLeft(buffer, width - 1, height - 1)
			};
			result.m_EffectiveBackgroundTolerance = EstimateBackgroundTolerance(buffer, backgroundColors, options.m_BackgroundTolerance, scratchArena);

			for (int32_t y = 0; y < height; ++y)
			{
				for (int32_t x = 0; x < width; ++x)
				{
					const size_t index = static_cast<size_t>(y) * width + x;
					candidateBackground[index] = IsNearAnyBackgroundColor(ReadPixelTopLeft(buffer, x, y), backgroundColors, result.m_EffectiveBackgroundTolerance) ? 1 : 0;
				}
			}

			for (size_t i = 0; i < pixelCount; ++i)
				result.m_ForegroundMask[i] = candidateBackground[i] ? 0 : 1;
		}
		RemoveForegroundSpeckles(result.m_ForegroundMask, width, height, scratchArena);

		memory::Vector<uint8_t> visited = memory::MakeVectorWithSize<uint8_t>(scratchArena, pixelCount, memory::MemoryTag::Temporary);
		memory::Vector<Bounds> components = memory::MakeVector<Bounds>(scratchArena, memory::MemoryTag::Temporary);
		memory::Deque<std::pair<int32_t, int32_t>> queue = memory::MakeDeque<std::pair<int32_t, int32_t>>(scratchArena, memory::MemoryTag::Temporary);
		static constexpr int32_t Directions4[4][2] =
		{
			{ 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 }
		};
		static constexpr int32_t Directions8[8][2] =
		{
			{ 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 },
			{ 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 }
		};
		const int32_t(*directions)[2] = options.m_SeparateDiagonalTouches ? Directions4 : Directions8;
		const size_t directionCount = options.m_SeparateDiagonalTouches ? std::size(Directions4) : std::size(Directions8);

		for (int32_t y = 0; y < height; ++y)
		{
			for (int32_t x = 0; x < width; ++x)
			{
				const size_t startIndex = static_cast<size_t>(y) * width + x;
				if (!result.m_ForegroundMask[startIndex] || visited[startIndex])
					continue;

				Bounds bounds;
				visited[startIndex] = 1;
				queue.emplace_back(x, y);
				while (!queue.empty())
				{
					const auto [cx, cy] = queue.front();
					queue.pop_front();
					bounds.m_MinX = std::min(bounds.m_MinX, cx);
					bounds.m_MinY = std::min(bounds.m_MinY, cy);
					bounds.m_MaxX = std::max(bounds.m_MaxX, cx);
					bounds.m_MaxY = std::max(bounds.m_MaxY, cy);
					++bounds.m_Pixels;

					for (size_t directionIndex = 0; directionIndex < directionCount; ++directionIndex)
					{
						const auto& direction = directions[directionIndex];
						const int32_t nx = cx + direction[0];
						const int32_t ny = cy + direction[1];
						if (nx < 0 || ny < 0 || nx >= width || ny >= height)
							continue;

						const size_t index = static_cast<size_t>(ny) * width + nx;
						if (!result.m_ForegroundMask[index] || visited[index])
							continue;

						visited[index] = 1;
						queue.emplace_back(nx, ny);
					}
				}

				const int32_t componentWidth = bounds.m_MaxX - bounds.m_MinX + 1;
				const int32_t componentHeight = bounds.m_MaxY - bounds.m_MinY + 1;
				if (bounds.m_Pixels >= options.m_MinPixels && componentWidth >= static_cast<int32_t>(options.m_MinSize) && componentHeight >= static_cast<int32_t>(options.m_MinSize))
					components.push_back(bounds);
			}
		}

		bool merged = true;
		const int32_t mergeGap = static_cast<int32_t>(options.m_MergeGap);
		while (merged)
		{
			merged = false;
			for (size_t i = 0; i < components.size() && !merged; ++i)
			{
				for (size_t j = i + 1; j < components.size(); ++j)
				{
					if (!ShouldMergeFragmentBounds(components[i], components[j], mergeGap, options.m_MinPixels))
						continue;

					MergeBounds(components[i], components[j]);
					components.erase(components.begin() + static_cast<std::ptrdiff_t>(j));
					merged = true;
					break;
				}
			}
		}

		std::ranges::sort(components, [](const Bounds& left, const Bounds& right)
			{
				const int32_t rowEpsilon = std::max(8, std::min(left.m_MaxY - left.m_MinY, right.m_MaxY - right.m_MinY) / 2);
				if (std::abs(left.m_MinY - right.m_MinY) > rowEpsilon)
					return left.m_MinY < right.m_MinY;
				return left.m_MinX < right.m_MinX;
			});

		const int32_t padding = static_cast<int32_t>(options.m_Padding);
		for (size_t i = 0; i < components.size(); ++i)
		{
			const Bounds& component = components[i];
			const int32_t minX = std::clamp(component.m_MinX - padding, 0, width - 1);
			const int32_t minY = std::clamp(component.m_MinY - padding, 0, height - 1);
			const int32_t maxX = std::clamp(component.m_MaxX + padding, 0, width - 1);
			const int32_t maxY = std::clamp(component.m_MaxY + padding, 0, height - 1);

			TextureSpriteRect sprite;
			char nameBuffer[128]{};
			std::snprintf(nameBuffer, sizeof(nameBuffer), "%s_%03zu", namePrefix.c_str(), i);
			sprite.m_Name = nameBuffer;
			sprite.m_X = static_cast<uint32_t>(minX);
			sprite.m_Y = static_cast<uint32_t>(minY);
			sprite.m_Width = static_cast<uint32_t>(maxX - minX + 1);
			sprite.m_Height = static_cast<uint32_t>(maxY - minY + 1);
			result.m_Sprites.push_back(std::move(sprite));
		}

		if (result.m_Sprites.empty())
			result.m_Error = "No separated sprites were detected. Try increasing background tolerance or lowering min pixels.";

		return result;
	}

	bool ExportSpritePngs(const PixelBuffer& buffer, const AutoSliceResult& result, const std::filesystem::path& outputDirectory, const std::string& namePrefix, std::vector<std::filesystem::path>& exportedPaths, std::string& error, uint32_t extrudePixels)
	{
		exportedPaths.clear();
		if (result.m_Sprites.empty() || result.m_ForegroundMask.size() != static_cast<size_t>(buffer.m_Width) * buffer.m_Height)
		{
			error = "Nothing to export.";
			return false;
		}

		std::error_code fsError;
		std::filesystem::create_directories(outputDirectory, fsError);
		if (fsError)
		{
			error = "Could not create slice output directory: " + fsError.message();
			return false;
		}

		memory::ArenaAllocator scratchArena(memory::Megabytes(2), &memory::GetAllocator(memory::MemoryTag::Asset), "TextureSlicerExportArena");
		for (size_t spriteIndex = 0; spriteIndex < result.m_Sprites.size(); ++spriteIndex)
		{
			scratchArena.Reset();
			const TextureSpriteRect& sprite = result.m_Sprites[spriteIndex];
			memory::Vector<uint8_t> crop = memory::MakeVectorWithSize<uint8_t>(scratchArena, static_cast<size_t>(sprite.m_Width) * sprite.m_Height * 4, memory::MemoryTag::Temporary);
			for (uint32_t y = 0; y < sprite.m_Height; ++y)
			{
				for (uint32_t x = 0; x < sprite.m_Width; ++x)
				{
					const uint32_t sourceX = sprite.m_X + x;
					const uint32_t sourceY = sprite.m_Y + y;
					const size_t sourceIndex = static_cast<size_t>(sourceY) * buffer.m_Width + sourceX;
					const size_t targetIndex = (static_cast<size_t>(y) * sprite.m_Width + x) * 4;
					if (!result.m_ForegroundMask[sourceIndex])
						continue;

					const Color color = ReadPixelTopLeft(buffer, static_cast<int32_t>(sourceX), static_cast<int32_t>(sourceY));
					crop[targetIndex + 0] = color.m_R;
					crop[targetIndex + 1] = color.m_G;
					crop[targetIndex + 2] = color.m_B;
					crop[targetIndex + 3] = color.m_A;
				}
			}

			ExtrudeTransparentRgb(crop, sprite.m_Width, sprite.m_Height, extrudePixels, scratchArena);

			char filenameBuffer[160]{};
			std::snprintf(filenameBuffer, sizeof(filenameBuffer), "%s_%03zu.png", namePrefix.c_str(), spriteIndex);
			const std::filesystem::path outputPath = outputDirectory / filenameBuffer;
			if (!WritePngRgbaTopLeft(outputPath, std::span<const uint8_t>(crop.data(), crop.size()), sprite.m_Width, sprite.m_Height, error, scratchArena))
				return false;
			exportedPaths.push_back(outputPath);
		}

		return true;
	}
}

_WHIP_END
