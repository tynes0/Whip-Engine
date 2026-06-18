#pragma once

#include <Whip/Asset/AssetMetadata.h>
#include <Whip/Core/Core.h>
#include <Whip/Render/Texture.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

_WHIP_START

namespace TextureSlicer
{
	struct PixelBuffer
	{
		uint32_t m_Width = 0;
		uint32_t m_Height = 0;
		uint32_t m_Channels = 4;
		ImageFormat m_Format = ImageFormat::None;
		std::vector<uint8_t> m_Pixels;
	};

	struct AutoSliceOptions
	{
		int32_t m_AlphaThreshold = 8;
		int32_t m_BackgroundTolerance = 24;
		uint32_t m_MinPixels = 24;
		uint32_t m_MinSize = 2;
		uint32_t m_Padding = 1;
		uint32_t m_MergeGap = 0;
		uint32_t m_ExtrudePixels = 0;
		bool m_SeparateDiagonalTouches = true;
	};

	struct AutoSliceResult
	{
		std::vector<TextureSpriteRect> m_Sprites;
		std::vector<uint8_t> m_ForegroundMask;
		bool m_UsedAlpha = false;
		int32_t m_EffectiveBackgroundTolerance = 0;
		std::string m_Error;
	};

	bool LoadTexturePixels(AssetHandle handle, PixelBuffer& buffer, std::string& error);
	AutoSliceResult DetectSprites(const PixelBuffer& buffer, const std::string& namePrefix, const AutoSliceOptions& options);
	bool ExportSpritePngs(const PixelBuffer& buffer, const AutoSliceResult& result, const std::filesystem::path& outputDirectory, const std::string& namePrefix, std::vector<std::filesystem::path>& exportedPaths, std::string& error, uint32_t extrudePixels = 0);
}

_WHIP_END
