#include "WhipPch.h"
#include <Whip/Render/Texture.h>

#include <Whip/Render/Renderer.h>
#include <Platform/OpenGL/OpenGLTexture.h>

_WHIP_START

Ref<Texture2D> Texture2D::Create(const TextureSpecification& specification, RawBuffer data)
{
	switch (Renderer::GetAPI())
	{
	case RenderAPI::API::None:		WHP_CORE_ASSERT(false, "RandererAPI is none!"); return nullptr;
	case RenderAPI::API::OpenGL:	return MakeRef<OpenGLTexture2D>(specification, data);
	}

	WHP_CORE_ASSERT(false, "Unknown RendererAPI");
	return nullptr;
}

Ref<Texture2D> Texture2D::CreateFromCoords(const Ref<Texture2D>& atlas, const glm::vec2& coords, const glm::vec2& cellSize, const glm::vec2& pixelSizeBetweenSprites, const glm::vec2& spriteSize)
{
	if (!atlas)
		return nullptr;
	// KoordinatlarÃƒÆ’Ã‚Â¯Ãƒâ€šÃ‚Â¿Ãƒâ€šÃ‚Â½ hesapla
	glm::vec2 emptyPixelSize = { pixelSizeBetweenSprites.x * coords.x, pixelSizeBetweenSprites.y * coords.y };
	uint32_t atlasWidth = atlas->GetWidth();
	uint32_t atlasHeight = atlas->GetHeight();

	glm::vec2 min = { ((coords.x * cellSize.x) + emptyPixelSize.x),
					  ((coords.y * cellSize.y) + emptyPixelSize.y) };
	glm::vec2 max = { (((coords.x + spriteSize.x) * cellSize.x) + emptyPixelSize.x),
					  (((coords.y + spriteSize.y) * cellSize.y) + emptyPixelSize.y) };

	uint32_t spriteWidth = static_cast<uint32_t>(max.x - min.x);
	uint32_t spriteHeight = static_cast<uint32_t>(max.y - min.y);

	// AtlasÃƒÆ’Ã‚Â¯Ãƒâ€šÃ‚Â¿Ãƒâ€šÃ‚Â½n ham verisini al
	RawBuffer atlasData = atlas->GetData();

	// Yeni sprite iÃƒÆ’Ã‚Â¯Ãƒâ€šÃ‚Â¿Ãƒâ€šÃ‚Â½in buffer oluÃƒÆ’Ã‚Â¯Ãƒâ€šÃ‚Â¿Ãƒâ€šÃ‚Â½tur
	RawBuffer spriteData(spriteWidth * spriteHeight * 4); // RGBA8 formatÃƒÆ’Ã‚Â¯Ãƒâ€šÃ‚Â¿Ãƒâ€šÃ‚Â½nÃƒÆ’Ã‚Â¯Ãƒâ€šÃ‚Â¿Ãƒâ€šÃ‚Â½ varsayÃƒÆ’Ã‚Â¯Ãƒâ€šÃ‚Â¿Ãƒâ€šÃ‚Â½yoruz

	// Veriyi atlas'tan yeni sprite'a kopyala
	for (uint32_t y = 0; y < spriteHeight; y++)
	{
		uint32_t atlasRowStart = (static_cast<uint32_t>(min.y) + y) * atlasWidth * 4 + static_cast<uint32_t>(min.x) * 4;
		uint32_t spriteRowStart = y * spriteWidth * 4;

		memcpy(spriteData.m_Data + spriteRowStart, atlasData.m_Data + atlasRowStart, spriteWidth * 4);
	}

	// Yeni Texture oluÃƒÆ’Ã‚Â¯Ãƒâ€šÃ‚Â¿Ãƒâ€šÃ‚Â½tur
	TextureSpecification spec;
	spec.m_Width = spriteWidth;
	spec.m_Height = spriteHeight;
	spec.m_Format = ImageFormat::Rgba8;

	auto result = Texture2D::Create(spec, spriteData);
	atlasData.Release();
	spriteData.Release();
	return result;
}

void Texture2D::FlipTextureBuffer(RawBuffer& buffer, int width, int height, int channels, FlipDirection direction)
{
	if (!buffer || buffer.m_Size < uint64_t(width * height * channels))
	{
		WHP_CORE_ERROR("[Texture 2D] Invalid buffer size or data!");
		return;
	}

	uint8_t* data = buffer.m_Data;

	if (direction & FlipDirectionHorizontal)
	{
		size_t bytesPerRow = (size_t)width * channels;
		StackBuffer<2048> temp{};
		RawBuffer bytes = buffer;

		for (int row = 0; row < height; row++) 
		{
			RawBuffer rowData(bytes.m_Data + row * bytesPerRow, 1);
			size_t left = 0;
			size_t right = size_t(width - 1);

			while (left < right)
			{
				uint8_t* leftPixel = rowData.m_Data + left * channels;
				uint8_t* rightPixel = rowData.m_Data + right * channels;

				size_t bytesCopy = (channels < temp.m_Size) ? channels : sizeof(temp);

				memcpy(temp.m_Data, leftPixel, bytesCopy);
				memcpy(leftPixel, rightPixel, bytesCopy);
				memcpy(rightPixel, temp.m_Data, bytesCopy);

				left++;
				right--;
			}
		}
	}

	if (direction & FlipDirectionVertical)
	{
		int row;
		size_t bytesPerRow = (size_t)width * channels;
		StackBuffer<2048> temp{};
		RawBuffer bytes = buffer;

		for (row = 0; row < (height >> 1); row++)
		{
			RawBuffer row0(bytes.m_Data + row * bytesPerRow, 1);
			RawBuffer row1(bytes.m_Data + (height - row - 1) * bytesPerRow, 1);
			size_t bytesLeft = bytesPerRow;
			while (bytesLeft) 
			{
				size_t bytesCopy = (bytesLeft < temp.m_Size) ? bytesLeft : sizeof(temp);
				memcpy(temp.m_Data, row0.m_Data, bytesCopy);
				memcpy(row0.m_Data, row1.m_Data, bytesCopy);
				memcpy(row1.m_Data, temp.m_Data, bytesCopy);
				row0.m_Data += bytesCopy;
				row1.m_Data += bytesCopy;
				bytesLeft -= bytesCopy;
			}
		}
	}
}

_WHIP_END
