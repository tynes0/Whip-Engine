#include "WhipPch.h"
#include <Whip/Render/Renderer2D.h>

#include <Whip/Render/VertexArray.h>
#include <Whip/Render/Shader.h>
#include <Whip/Render/RenderCommand.h>
#include <Whip/Render/UniformBuffer.h>
#include <Whip/Render/MsdfData.h>

#include <Whip/Asset/AssetManager.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <array>
#include <deque>
#include <limits>
#include <unordered_map>
#include <utility>
#include <vector>

_WHIP_START

namespace
{
	struct SpriteTextureCacheEntry
	{
		RendererId m_SourceRendererId = 0;
		TextureSpriteRect m_Rect;
		TextureSpriteMode m_Mode = TextureSpriteMode::Single;
		Ref<SubTexture2D> m_SubTexture;
	};

	struct TexturePixelDataCacheEntry
	{
		RendererId m_SourceRendererId = 0;
		RawBuffer m_Data;
	};

	memory::UnorderedMap<uint64_t, SpriteTextureCacheEntry> s_IsolatedSpriteTextureCache;
	memory::UnorderedMap<uint64_t, Ref<SubTexture2D>> s_FrameSubTextureCache;
	memory::UnorderedMap<AssetHandle, TexturePixelDataCacheEntry> s_TexturePixelDataCache;

	uint64_t MakeSpriteTextureCacheKey(AssetHandle textureHandle, int32_t spriteIndex)
	{
		uint64_t seed = static_cast<uint64_t>(textureHandle);
		const uint64_t value = static_cast<uint64_t>(static_cast<uint32_t>(spriteIndex));
		seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6U) + (seed >> 2U);
		return seed;
	}

	size_t PixelIndexBottomLeft(uint32_t width, uint32_t x, uint32_t storageY)
	{
		return (static_cast<size_t>(storageY) * width + x) * 4;
	}

	size_t PixelIndexTopLeft(uint32_t width, uint32_t height, uint32_t x, uint32_t topLeftY)
	{
		return PixelIndexBottomLeft(width, x, height - 1U - topLeftY);
	}

	bool SameSpriteRect(const TextureSpriteRect& left, const TextureSpriteRect& right)
	{
		return left.m_X == right.m_X &&
			left.m_Y == right.m_Y &&
			left.m_Width == right.m_Width &&
			left.m_Height == right.m_Height &&
			left.m_Name == right.m_Name;
	}

	void ExtrudeTransparentRgb(memory::Vector<uint8_t>& rgbaPixels, uint32_t width, uint32_t height, uint32_t passes)
	{
		if (passes == 0 || width == 0 || height == 0 || rgbaPixels.size() != static_cast<size_t>(width) * height * 4)
			return;

		memory::Vector<uint8_t> filled = memory::MakeFrameVectorWithSize<uint8_t>(static_cast<size_t>(width) * height);
		for (uint32_t y = 0; y < height; ++y)
			for (uint32_t x = 0; x < width; ++x)
				filled[static_cast<size_t>(y) * width + x] = rgbaPixels[PixelIndexBottomLeft(width, x, y) + 3] > 0 ? 1 : 0;

		static constexpr int32_t Directions4[4][2] =
		{
			{ 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 }
		};

		for (uint32_t pass = 0; pass < passes; ++pass)
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

						const size_t neighborMaskIndex = static_cast<size_t>(ny) * width + static_cast<uint32_t>(nx);
						if (!filled[neighborMaskIndex])
							continue;

						const size_t target = PixelIndexBottomLeft(width, x, y);
						const size_t source = PixelIndexBottomLeft(width, static_cast<uint32_t>(nx), static_cast<uint32_t>(ny));
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

	Ref<SubTexture2D> CreateUvSubTextureFromSpriteRect(const Ref<Texture2D>& texture, const TextureSpriteRect& sprite)
	{
		const float textureWidth = static_cast<float>(texture->GetWidth());
		const float textureHeight = static_cast<float>(texture->GetHeight());
		const float insetX = sprite.m_Width > 2 ? 0.5f : 0.0f;
		const float insetY = sprite.m_Height > 2 ? 0.5f : 0.0f;
		const float minX = std::clamp((static_cast<float>(sprite.m_X) + insetX) / textureWidth, 0.0f, 1.0f);
		const float maxX = std::clamp((static_cast<float>(sprite.m_X + sprite.m_Width) - insetX) / textureWidth, 0.0f, 1.0f);
		const float minY = std::clamp(1.0f - (static_cast<float>(sprite.m_Y + sprite.m_Height) - insetY) / textureHeight, 0.0f, 1.0f);
		const float maxY = std::clamp(1.0f - (static_cast<float>(sprite.m_Y) + insetY) / textureHeight, 0.0f, 1.0f);
		return MakeRefTagged<SubTexture2D>(memory::MemoryTag::Renderer, texture, glm::vec2(minX, minY), glm::vec2(maxX, maxY));
	}

	const RawBuffer* GetCachedTexturePixels(const Ref<Texture2D>& texture, AssetHandle textureHandle)
	{
		if (!texture || texture->GetSpecification().m_Format != ImageFormat::Rgba8 || texture->GetWidth() == 0 || texture->GetHeight() == 0)
			return nullptr;

		const uint64_t expectedSize = static_cast<uint64_t>(texture->GetWidth()) * texture->GetHeight() * 4;
		auto it = s_TexturePixelDataCache.try_emplace(textureHandle).first;
		TexturePixelDataCacheEntry& entry = it->second;
		if (entry.m_SourceRendererId != texture->GetRendererId() || !entry.m_Data || entry.m_Data.m_Size != expectedSize)
		{
			entry.m_Data.Release();
			entry.m_Data = texture->GetData();
			entry.m_SourceRendererId = texture->GetRendererId();
		}

		if (!entry.m_Data || entry.m_Data.m_Size != expectedSize)
			return nullptr;
		return &entry.m_Data;
	}

	void ReleaseTexturePixelDataCache()
	{
		for (auto& [handle, entry] : s_TexturePixelDataCache)
			entry.m_Data.Release();
		s_TexturePixelDataCache.clear();
	}

	Ref<SubTexture2D> CreateIsolatedSubTextureFromSpriteRect(const Ref<Texture2D>& texture, AssetHandle textureHandle, const TextureSpriteRect& sprite)
	{
		if (texture->GetSpecification().m_Format != ImageFormat::Rgba8)
			return nullptr;

		const uint32_t textureWidth = texture->GetWidth();
		const uint32_t textureHeight = texture->GetHeight();
		if (sprite.m_X >= textureWidth || sprite.m_Y >= textureHeight)
			return nullptr;

		const uint32_t cropWidth = std::min(sprite.m_Width, textureWidth - sprite.m_X);
		const uint32_t cropHeight = std::min(sprite.m_Height, textureHeight - sprite.m_Y);
		if (cropWidth == 0 || cropHeight == 0)
			return nullptr;

		const RawBuffer* sourceData = GetCachedTexturePixels(texture, textureHandle);
		if (!sourceData)
			return nullptr;

		memory::Vector<uint8_t> foreground = memory::MakeFrameVectorWithSize<uint8_t>(static_cast<size_t>(cropWidth) * cropHeight);
		for (uint32_t y = 0; y < cropHeight; ++y)
		{
			for (uint32_t x = 0; x < cropWidth; ++x)
			{
				const size_t sourceIndex = PixelIndexTopLeft(textureWidth, textureHeight, sprite.m_X + x, sprite.m_Y + y);
				foreground[static_cast<size_t>(y) * cropWidth + x] = sourceData->m_Data[sourceIndex + 3] > 8 ? 1 : 0;
			}
		}

		memory::Vector<uint8_t> visited = memory::MakeFrameVectorWithSize<uint8_t>(foreground.size());
		memory::Vector<uint8_t> selected = memory::MakeFrameVectorWithSize<uint8_t>(foreground.size());
		memory::Deque<std::pair<uint32_t, uint32_t>> queue = memory::MakeFrameDeque<std::pair<uint32_t, uint32_t>>();
		memory::Vector<size_t> componentPixels = memory::MakeFrameVector<size_t>();
		memory::Vector<size_t> bestComponentPixels = memory::MakeFrameVector<size_t>();
		static constexpr int32_t Directions4[4][2] =
		{
			{ 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 }
		};

		for (uint32_t y = 0; y < cropHeight; ++y)
		{
			for (uint32_t x = 0; x < cropWidth; ++x)
			{
				const size_t startIndex = static_cast<size_t>(y) * cropWidth + x;
				if (!foreground[startIndex] || visited[startIndex])
					continue;

				componentPixels.clear();
				visited[startIndex] = 1;
				queue.emplace_back(x, y);
				while (!queue.empty())
				{
					const auto [cx, cy] = queue.front();
					queue.pop_front();
					const size_t pixelIndex = static_cast<size_t>(cy) * cropWidth + cx;
					componentPixels.push_back(pixelIndex);

					for (const auto& direction : Directions4)
					{
						const int32_t nx = static_cast<int32_t>(cx) + direction[0];
						const int32_t ny = static_cast<int32_t>(cy) + direction[1];
						if (nx < 0 || ny < 0 || nx >= static_cast<int32_t>(cropWidth) || ny >= static_cast<int32_t>(cropHeight))
							continue;

						const size_t neighborIndex = static_cast<size_t>(ny) * cropWidth + static_cast<uint32_t>(nx);
						if (!foreground[neighborIndex] || visited[neighborIndex])
							continue;

						visited[neighborIndex] = 1;
						queue.emplace_back(static_cast<uint32_t>(nx), static_cast<uint32_t>(ny));
					}
				}

				if (componentPixels.size() > bestComponentPixels.size())
					bestComponentPixels = componentPixels;
			}
		}

		if (bestComponentPixels.empty())
			return nullptr;

		for (size_t pixelIndex : bestComponentPixels)
			selected[pixelIndex] = 1;

		memory::Vector<uint8_t> cropPixels = memory::MakeFrameVectorWithSize<uint8_t>(static_cast<size_t>(cropWidth) * cropHeight * 4);
		for (uint32_t y = 0; y < cropHeight; ++y)
		{
			for (uint32_t x = 0; x < cropWidth; ++x)
			{
				const size_t maskIndex = static_cast<size_t>(y) * cropWidth + x;
				if (!selected[maskIndex])
					continue;

				const size_t sourceIndex = PixelIndexTopLeft(textureWidth, textureHeight, sprite.m_X + x, sprite.m_Y + y);
				const size_t targetIndex = PixelIndexTopLeft(cropWidth, cropHeight, x, y);
				cropPixels[targetIndex + 0] = sourceData->m_Data[sourceIndex + 0];
				cropPixels[targetIndex + 1] = sourceData->m_Data[sourceIndex + 1];
				cropPixels[targetIndex + 2] = sourceData->m_Data[sourceIndex + 2];
				cropPixels[targetIndex + 3] = sourceData->m_Data[sourceIndex + 3];
			}
		}

		ExtrudeTransparentRgb(cropPixels, cropWidth, cropHeight, 2);

		TextureSpecification specification;
		specification.m_Width = cropWidth;
		specification.m_Height = cropHeight;
		specification.m_Format = ImageFormat::Rgba8;
		specification.m_FilterMode = texture->GetSpecification().m_FilterMode;
		specification.m_WrapMode = TextureWrapMode::ClampToEdge;
		specification.m_GenerateMips = false;

		Ref<Texture2D> isolatedTexture = Texture2D::Create(specification, RawBuffer(cropPixels.data(), cropPixels.size()));
		if (!isolatedTexture || !isolatedTexture->IsLoaded())
			return nullptr;

		return MakeRefTagged<SubTexture2D>(memory::MemoryTag::Renderer, isolatedTexture, glm::vec2(0.0f), glm::vec2(1.0f));
	}

	Ref<SubTexture2D> CreateSubTextureFromSpriteIndex(const Ref<Texture2D>& texture, AssetHandle textureHandle, int32_t spriteIndex)
	{
		if (!texture || spriteIndex < 0)
			return nullptr;

		const uint64_t cacheKey = MakeSpriteTextureCacheKey(textureHandle, spriteIndex);
		if (auto frameIt = s_FrameSubTextureCache.find(cacheKey); frameIt != s_FrameSubTextureCache.end())
			return frameIt->second;

		if (!AssetManager::IsAssetHandleValid(textureHandle))
			return nullptr;

		const AssetMetadata& metadata = AssetManager::GetAssetMetadata(textureHandle);
		const auto& sprites = metadata.m_TextureSettings.m_Sprites;
		if (spriteIndex < 0 || spriteIndex >= static_cast<int32_t>(sprites.size()))
			return nullptr;

		const TextureSpriteRect& sprite = sprites[static_cast<size_t>(spriteIndex)];
		if (sprite.m_Width == 0 || sprite.m_Height == 0 || texture->GetWidth() == 0 || texture->GetHeight() == 0)
			return nullptr;

		const RendererId sourceRendererId = texture->GetRendererId();
		const TextureSpriteMode spriteMode = metadata.m_TextureSettings.m_SpriteMode;
		auto it = s_IsolatedSpriteTextureCache.find(cacheKey);
		if (it != s_IsolatedSpriteTextureCache.end() &&
			it->second.m_SourceRendererId == sourceRendererId &&
			it->second.m_Mode == spriteMode &&
			SameSpriteRect(it->second.m_Rect, sprite) &&
			it->second.m_SubTexture)
		{
			s_FrameSubTextureCache[cacheKey] = it->second.m_SubTexture;
			return it->second.m_SubTexture;
		}

		if (metadata.m_TextureSettings.m_SpriteMode == TextureSpriteMode::Multiple)
		{
			if (Ref<SubTexture2D> isolatedSubTexture = CreateIsolatedSubTextureFromSpriteRect(texture, textureHandle, sprite))
			{
				s_IsolatedSpriteTextureCache[cacheKey] = { sourceRendererId, sprite, spriteMode, isolatedSubTexture };
				s_FrameSubTextureCache[cacheKey] = isolatedSubTexture;
				return isolatedSubTexture;
			}
		}

		Ref<SubTexture2D> uvSubTexture = CreateUvSubTextureFromSpriteRect(texture, sprite);
		if (uvSubTexture)
		{
			s_IsolatedSpriteTextureCache[cacheKey] = { sourceRendererId, sprite, spriteMode, uvSubTexture };
			s_FrameSubTextureCache[cacheKey] = uvSubTexture;
		}
		return uvSubTexture;
	}
}

struct QuadVertex
{
	glm::vec3 m_Position;
	glm::vec4 m_Color;
	glm::vec2 m_TextureCoord;
	float m_TextureIndex;
	float m_TilingFactor;
	int m_EntityId; // editor only
};

struct CircleVertex
{
	glm::vec3 m_WorldPosition;
	glm::vec3 m_LocalPosition;
	glm::vec4 m_Color;
	float m_Thickness;
	float m_Fade;
	int m_EntityId; // editor only
};

struct LineVertex
{
	glm::vec3 m_Position;
	glm::vec4 m_Color;
	int m_EntityId; // editor only
};

struct TextVertex
{
	glm::vec3 m_Position;
	glm::vec4 m_Color;
	glm::vec2 m_TextureCoord;
	// TODO: bg color for outline/bg
	int m_EntityId; // editor only
};

struct Renderer2DData
{
	static constexpr uint32_t MaxQuads = 20000;
	static constexpr uint32_t MaxVertices = MaxQuads * 4;
	static constexpr uint32_t MaxIndices = MaxQuads * 6;
	static constexpr uint32_t MaxTextureSlots = 32; // TODO: render_caps

	Ref<VertexArray> m_QuadVertexArray;
	Ref<VertexBuffer> m_QuadVertexBuffer;
	Ref<Shader> m_QuadShader;
	Ref<Texture2D> m_WhiteTexture;

	Ref<VertexArray> m_CircleVertexArray;
	Ref<VertexBuffer> m_CircleVertexBuffer;
	Ref<Shader> m_CircleShader;

	Ref<VertexArray> m_LineVertexArray;
	Ref<VertexBuffer> m_LineVertexBuffer;
	Ref<Shader> m_LineShader;

	Ref<VertexArray> m_TextVertexArray;
	Ref<VertexBuffer> m_TextVertexBuffer;
	Ref<Shader> m_TextShader;

	uint32_t m_QuadIndexCount = 0;
	QuadVertex* m_QuadVertexBufferBase = nullptr;
	QuadVertex* m_QuadVertexBufferPtr = nullptr;

	uint32_t m_CircleIndexCount = 0;
	CircleVertex* m_CircleVertexBufferBase = nullptr;
	CircleVertex* m_CircleVertexBufferPtr = nullptr;

	uint32_t m_LineVertexCount = 0;
	LineVertex* m_LineVertexBufferBase = nullptr;
	LineVertex* m_LineVertexBufferPtr = nullptr;

	uint32_t m_TextIndexCount = 0;
	TextVertex* m_TextVertexBufferBase = nullptr;
	TextVertex* m_TextVertexBufferPtr = nullptr;

	std::array <Ref<Texture2D>, MaxTextureSlots> m_TextureSlots;
	uint32_t m_TextureSlotIndex = 1; // 0 = white Texture

	Ref<Texture2D> m_FontAtlasTexture;
	memory::UnorderedMap<AssetHandle, Ref<Texture2D>> m_TextureAssetCache;

	glm::vec4 m_QuadVertexPositions[4] = {};

	Renderer2D::Statistics m_Stats;

	struct CameraData
	{
		glm::mat4 m_ViewProjection;
	};
	CameraData m_CameraBuffer{};
	Ref<UniformBuffer> m_CameraUniformBuffer;
};


namespace
{
	Renderer2DData s_Data;

	Ref<Texture2D> GetFrameCachedTexture(AssetHandle handle)
	{
		if (handle == 0)
			return nullptr;

		if (auto it = s_Data.m_TextureAssetCache.find(handle); it != s_Data.m_TextureAssetCache.end())
			return it->second;

		if (!AssetManager::IsAssetHandleValid(handle))
			return nullptr;

		Ref<Texture2D> texture = AssetManager::GetAsset<Texture2D>(handle);
		if (texture)
			s_Data.m_TextureAssetCache[handle] = texture;
		return texture;
	}

	template <typename T>
	void ReleaseVertexBuffer(T*& buffer)
	{
		if (buffer)
			memory::DeleteArray(memory::GetAllocator(memory::MemoryTag::Renderer), buffer);
		buffer = nullptr;
	}

	template <typename T>
	T* AllocateVertexBuffer(size_t count)
	{
		return memory::NewArray<T>(memory::GetAllocator(memory::MemoryTag::Renderer), count, memory::MemoryTag::Renderer, WHIP_MEMORY_LOCATION);
	}

	void SetAndIncrementQuadVertexBufferPtr(const glm::vec3& position, const glm::vec4& color, glm::vec2 textureCoord, float textureIndex, float tilingFactor, int entityId)
	{
		s_Data.m_QuadVertexBufferPtr->m_Position = position;
		s_Data.m_QuadVertexBufferPtr->m_Color = color;
		s_Data.m_QuadVertexBufferPtr->m_TextureCoord = textureCoord;
		s_Data.m_QuadVertexBufferPtr->m_TextureIndex = textureIndex;
		s_Data.m_QuadVertexBufferPtr->m_TilingFactor = tilingFactor;
		s_Data.m_QuadVertexBufferPtr->m_EntityId = entityId;
		s_Data.m_QuadVertexBufferPtr++;
	}
}

void Renderer2D::Init()
{
	WHP_PROFILE_FUNCTION();

	s_Data.m_QuadVertexArray = VertexArray::Create();
	s_Data.m_QuadVertexBuffer = VertexBuffer::Create(s_Data.MaxVertices * sizeof(QuadVertex));
	s_Data.m_QuadVertexBuffer->SetLayout({
			{whip::ShaderDataType::Float3, "a_position"},
			{whip::ShaderDataType::Float4, "a_color"},
			{whip::ShaderDataType::Float2, "a_texture_coord"},
			{whip::ShaderDataType::Float,  "a_texture_index"},
			{whip::ShaderDataType::Float,  "a_tiling_factor"},
			{whip::ShaderDataType::Int,  "a_entityID"}
		});
	s_Data.m_QuadVertexArray->AddVertexBuffer(s_Data.m_QuadVertexBuffer);

	s_Data.m_QuadVertexBufferBase = AllocateVertexBuffer<QuadVertex>(s_Data.MaxVertices);

	uint32_t* quadIndices = AllocateVertexBuffer<uint32_t>(s_Data.MaxIndices);

	uint32_t offset = 0;

	_WHP_PRAGMA_WARNING(push)
		_WHP_PRAGMA_WARNING_DISABLE(6386)

		for (uint32_t i = 0; i < s_Data.MaxIndices; i += 6)
		{
			quadIndices[i + 0] = offset + 0;
			quadIndices[i + 1] = offset + 1;
			quadIndices[i + 2] = offset + 2;
			quadIndices[i + 3] = offset + 2;
			quadIndices[i + 4] = offset + 3;
			quadIndices[i + 5] = offset + 0;

			offset += 4;
		}

	_WHP_PRAGMA_WARNING(pop)

	Ref<IndexBuffer> quadIndexBuffer = IndexBuffer::Create(quadIndices, s_Data.MaxIndices);
	s_Data.m_QuadVertexArray->SetIndexBuffer(quadIndexBuffer);
	ReleaseVertexBuffer(quadIndices);

	// Circles
	s_Data.m_CircleVertexArray = VertexArray::Create();

	s_Data.m_CircleVertexBuffer = VertexBuffer::Create(s_Data.MaxVertices * sizeof(CircleVertex));
	s_Data.m_CircleVertexBuffer->SetLayout({
		{ ShaderDataType::Float3, "a_world_position" },
		{ ShaderDataType::Float3, "a_local_position" },
		{ ShaderDataType::Float4, "a_color"         },
		{ ShaderDataType::Float,  "a_thickness"     },
		{ ShaderDataType::Float,  "a_fade"          },
		{ ShaderDataType::Int,    "a_entityID"      }
		});
	s_Data.m_CircleVertexArray->AddVertexBuffer(s_Data.m_CircleVertexBuffer);
	s_Data.m_CircleVertexArray->SetIndexBuffer(quadIndexBuffer);
	s_Data.m_CircleVertexBufferBase = AllocateVertexBuffer<CircleVertex>(s_Data.MaxVertices);

	// lines
	s_Data.m_LineVertexArray = VertexArray::Create();

	s_Data.m_LineVertexBuffer = VertexBuffer::Create(s_Data.MaxVertices * sizeof(LineVertex));
	s_Data.m_LineVertexBuffer->SetLayout({
		{ ShaderDataType::Float3, "a_position" },
		{ ShaderDataType::Float4, "a_color"    },
		{ ShaderDataType::Int,    "a_entityID" }
		});
	s_Data.m_LineVertexArray->AddVertexBuffer(s_Data.m_LineVertexBuffer);
	s_Data.m_LineVertexBufferBase = AllocateVertexBuffer<LineVertex>(s_Data.MaxVertices);

	// texts
	s_Data.m_TextVertexArray = VertexArray::Create();

	s_Data.m_TextVertexBuffer = VertexBuffer::Create(s_Data.MaxVertices * sizeof(TextVertex));
	s_Data.m_TextVertexBuffer->SetLayout({
		{ ShaderDataType::Float3, "a_position"     },
		{ ShaderDataType::Float4, "a_color"        },
		{ ShaderDataType::Float2, "a_texture_coord"},
		{ ShaderDataType::Int,    "a_entityID"     }
		});
	s_Data.m_TextVertexArray->AddVertexBuffer(s_Data.m_TextVertexBuffer);
	s_Data.m_TextVertexArray->SetIndexBuffer(quadIndexBuffer);
	s_Data.m_TextVertexBufferBase = AllocateVertexBuffer<TextVertex>(s_Data.MaxVertices);

	s_Data.m_WhiteTexture = Texture2D::Create(TextureSpecification{});
	uint32_t whiteTextureData = 0xffffffff;
	s_Data.m_WhiteTexture->SetData(RawBuffer(&whiteTextureData, sizeof(whiteTextureData)));

	s_Data.m_QuadShader		= Shader::Create("assets\\shaders\\renderer2D_quad.glsl");
	s_Data.m_CircleShader	= Shader::Create("assets\\shaders\\renderer2D_circle.glsl");
	s_Data.m_LineShader		= Shader::Create("assets\\shaders\\renderer2D_line.glsl");
	s_Data.m_TextShader		= Shader::Create("assets\\shaders\\renderer2D_text.glsl");

	s_Data.m_TextureSlots[0] = s_Data.m_WhiteTexture;

	s_Data.m_QuadVertexPositions[0] = { -0.5f, -0.5f, 0.0f, 1.0f };
	s_Data.m_QuadVertexPositions[1] = {  0.5f, -0.5f, 0.0f, 1.0f };
	s_Data.m_QuadVertexPositions[2] = {  0.5f,	0.5f, 0.0f, 1.0f };
	s_Data.m_QuadVertexPositions[3] = { -0.5f,  0.5f, 0.0f, 1.0f };

	s_Data.m_CameraUniformBuffer = UniformBuffer::Create(sizeof(Renderer2DData::CameraData), 0);

	SetLineWidth(2);
}

void Renderer2D::Shutdown()
{
	WHP_PROFILE_FUNCTION();

	s_IsolatedSpriteTextureCache.clear();
	s_FrameSubTextureCache.clear();
	ReleaseTexturePixelDataCache();
	s_Data.m_TextureAssetCache.clear();

	ReleaseVertexBuffer(s_Data.m_QuadVertexBufferBase);
	ReleaseVertexBuffer(s_Data.m_CircleVertexBufferBase);
	ReleaseVertexBuffer(s_Data.m_LineVertexBufferBase);
	ReleaseVertexBuffer(s_Data.m_TextVertexBufferBase);

	s_Data.m_QuadVertexBufferPtr = nullptr;
	s_Data.m_CircleVertexBufferPtr = nullptr;
	s_Data.m_LineVertexBufferPtr = nullptr;
	s_Data.m_TextVertexBufferPtr = nullptr;

	for (Ref<Texture2D>& textureSlot : s_Data.m_TextureSlots)
		textureSlot.reset();

	s_Data.m_FontAtlasTexture.reset();
	s_Data.m_WhiteTexture.reset();

	s_Data.m_QuadShader.reset();
	s_Data.m_CircleShader.reset();
	s_Data.m_LineShader.reset();
	s_Data.m_TextShader.reset();

	s_Data.m_QuadVertexBuffer.reset();
	s_Data.m_CircleVertexBuffer.reset();
	s_Data.m_LineVertexBuffer.reset();
	s_Data.m_TextVertexBuffer.reset();

	s_Data.m_QuadVertexArray.reset();
	s_Data.m_CircleVertexArray.reset();
	s_Data.m_LineVertexArray.reset();
	s_Data.m_TextVertexArray.reset();

	s_Data.m_CameraUniformBuffer.reset();

	s_Data.m_QuadIndexCount = 0;
	s_Data.m_CircleIndexCount = 0;
	s_Data.m_LineVertexCount = 0;
	s_Data.m_TextIndexCount = 0;
	s_Data.m_TextureSlotIndex = 1;
}

void Renderer2D::BeginScene(const OrthographicCamera& camera)
{
	WHP_PROFILE_FUNCTION();

	s_Data.m_CameraBuffer.m_ViewProjection = camera.GetViewProjectionMatrix();
	s_Data.m_CameraUniformBuffer->SetData(&s_Data.m_CameraBuffer, sizeof(Renderer2DData::CameraData));

	s_Data.m_TextureAssetCache.clear();
	s_FrameSubTextureCache.clear();
	StartBatch();
}

void Renderer2D::BeginScene(const Camera& cam, const glm::mat4& transform)
{
	WHP_PROFILE_FUNCTION();

	s_Data.m_CameraBuffer.m_ViewProjection = cam.GetProjection() * glm::inverse(transform);
	s_Data.m_CameraUniformBuffer->SetData(&s_Data.m_CameraBuffer, sizeof(Renderer2DData::CameraData));

	s_Data.m_TextureAssetCache.clear();
	s_FrameSubTextureCache.clear();
	StartBatch();
}

void Renderer2D::BeginScene(const EditorCamera& cam)
{
	WHP_PROFILE_FUNCTION();

	s_Data.m_CameraBuffer.m_ViewProjection = cam.GetViewProjection();
	s_Data.m_CameraUniformBuffer->SetData(&s_Data.m_CameraBuffer, sizeof(Renderer2DData::CameraData));

	s_Data.m_TextureAssetCache.clear();
	s_FrameSubTextureCache.clear();
	StartBatch();
}

void Renderer2D::EndScene()
{
	WHP_PROFILE_FUNCTION();
	Flush();
}

void Renderer2D::Flush()
{
	if (s_Data.m_QuadIndexCount)
	{
		uint32_t dataSize = static_cast<uint32_t>(reinterpret_cast<uint8_t*>(s_Data.m_QuadVertexBufferPtr) - reinterpret_cast<uint8_t*>(s_Data.m_QuadVertexBufferBase));
		s_Data.m_QuadVertexBuffer->SetData(s_Data.m_QuadVertexBufferBase, dataSize);
		// Bind textures
		for (uint32_t i = 0; i < s_Data.m_TextureSlotIndex; i++)
			s_Data.m_TextureSlots[i]->Bind(i);

		s_Data.m_QuadShader->Bind();
		RenderCommand::DrawIndexed(s_Data.m_QuadVertexArray, s_Data.m_QuadIndexCount);
	s_Data.m_Stats.m_DrawCalls++;
	}

	if (s_Data.m_CircleIndexCount)
	{
		uint32_t dataSize = static_cast<uint32_t>(reinterpret_cast<uint8_t*>(s_Data.m_CircleVertexBufferPtr) - reinterpret_cast<uint8_t*>(s_Data.m_CircleVertexBufferBase));
		s_Data.m_CircleVertexBuffer->SetData(s_Data.m_CircleVertexBufferBase, dataSize);

		s_Data.m_CircleShader->Bind();
		RenderCommand::DrawIndexed(s_Data.m_CircleVertexArray, s_Data.m_CircleIndexCount);
	s_Data.m_Stats.m_DrawCalls++;
	}

	if (s_Data.m_LineVertexCount)
	{
		uint32_t dataSize = static_cast<uint32_t>(reinterpret_cast<uint8_t*>(s_Data.m_LineVertexBufferPtr) - reinterpret_cast<uint8_t*>(s_Data.m_LineVertexBufferBase));
		s_Data.m_LineVertexBuffer->SetData(s_Data.m_LineVertexBufferBase, dataSize);

		s_Data.m_LineShader->Bind();
		RenderCommand::DrawLines(s_Data.m_LineVertexArray, s_Data.m_LineVertexCount);
	s_Data.m_Stats.m_DrawCalls++;
	}

	if (s_Data.m_TextIndexCount)
	{
		uint32_t dataSize = static_cast<uint32_t>(reinterpret_cast<uint8_t*>(s_Data.m_TextVertexBufferPtr) - reinterpret_cast<uint8_t*>(s_Data.m_TextVertexBufferBase));
		s_Data.m_TextVertexBuffer->SetData(s_Data.m_TextVertexBufferBase, dataSize);

		s_Data.m_FontAtlasTexture->Bind(0);

		s_Data.m_TextShader->Bind();
		RenderCommand::DrawIndexed(s_Data.m_TextVertexArray, s_Data.m_TextIndexCount);
	s_Data.m_Stats.m_DrawCalls++;
	}
}


void Renderer2D::DrawQuad(const glm::mat4& transform, const glm::vec4& color, int entityId)
{
	WHP_PROFILE_HOT_FUNCTION();

	if (s_Data.m_QuadIndexCount >= Renderer2DData::MaxIndices)
		NextBatch();

	constexpr size_t QuadVertexCount = 4u;
	constexpr float textureIndex = 0.0f; // white color
	constexpr float tilingFactor = 1.0f;
	constexpr glm::vec2 textureCoords[] = { {0.0f,0.0f},{1.0f,0.0f},{1.0f,1.0f},{0.0f,1.0f} };

	for (size_t i = 0; i < QuadVertexCount; ++i)
		SetAndIncrementQuadVertexBufferPtr(transform * s_Data.m_QuadVertexPositions[i], color, textureCoords[i], textureIndex, tilingFactor, entityId);
	s_Data.m_QuadIndexCount += 6;
	s_Data.m_Stats.m_QuadCounts++;
}

void Renderer2D::DrawQuad(const glm::mat4& transform, const Ref<Texture2D>& tex, float tilingFactor, const glm::vec4& tintColor, int entityId)
{
	WHP_PROFILE_HOT_FUNCTION();
	WHP_CORE_VERIFY(tex)

	if (s_Data.m_QuadIndexCount >= Renderer2DData::MaxIndices)
		NextBatch();

	constexpr size_t QuadVertexCount = 4u;
	constexpr glm::vec2 textureCoords[] = { {0.0f,0.0f},{1.0f,0.0f},{1.0f,1.0f},{0.0f,1.0f} };

	float textureIndex = 0.0f;

	for (uint32_t i = 1; i < s_Data.m_TextureSlotIndex; ++i)
		if (DREF(s_Data.m_TextureSlots[i]) == DREF(tex))
		{
			textureIndex = static_cast<float>(i);
			break;
		}

_WHP_PRAGMA_WARNING(push)
_WHP_PRAGMA_WARNING_DISABLE(28020)
	if (textureIndex == 0.0f)
	{
		if (s_Data.m_TextureSlotIndex >= Renderer2DData::MaxTextureSlots)
			NextBatch();
		textureIndex = static_cast<float>(s_Data.m_TextureSlotIndex);
		s_Data.m_TextureSlots[s_Data.m_TextureSlotIndex] = tex;
		s_Data.m_TextureSlotIndex++;
	}
_WHP_PRAGMA_WARNING(pop)

	for (size_t i = 0; i < QuadVertexCount; ++i)
		SetAndIncrementQuadVertexBufferPtr(transform * s_Data.m_QuadVertexPositions[i], tintColor, textureCoords[i], textureIndex, tilingFactor, entityId);

	s_Data.m_QuadIndexCount += 6;
	s_Data.m_Stats.m_QuadCounts++;
}

void Renderer2D::DrawQuad(const glm::mat4& transform, const Ref<SubTexture2D>& subTexture, float tilingFactor, const glm::vec4& tintColor, int entityId)
{
	WHP_PROFILE_HOT_FUNCTION();

	if (s_Data.m_QuadIndexCount >= Renderer2DData::MaxIndices)
		NextBatch();

	constexpr size_t QuadVertexCount = 4u;
	const glm::vec2* textureCoords = subTexture->GetTextureCoords();
	const Ref<Texture2D>& tex = subTexture->GetTexture();

	float textureIndex = 0.0f;

	for (uint32_t i = 1; i < s_Data.m_TextureSlotIndex; ++i)
		if (DREF(s_Data.m_TextureSlots[i].get()) == DREF(tex.get()))
		{
			textureIndex = static_cast<float>(i);
			break;
		}

	if (textureIndex == 0.0f)
	{
		if (s_Data.m_TextureSlotIndex >= Renderer2DData::MaxTextureSlots)
			NextBatch();
		textureIndex = static_cast<float>(s_Data.m_TextureSlotIndex);
		s_Data.m_TextureSlots[s_Data.m_TextureSlotIndex++] = tex;
	}

	for (size_t i = 0; i < QuadVertexCount; ++i)
		SetAndIncrementQuadVertexBufferPtr(transform * s_Data.m_QuadVertexPositions[i], tintColor, textureCoords[i], textureIndex, tilingFactor, entityId);

	s_Data.m_QuadIndexCount += 6;
	s_Data.m_Stats.m_QuadCounts++;
}


void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color)
{
	DrawQuad({ position.x, position.y, 0.0f }, size, color);
}

void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color)
{
	glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
		* glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });
	DrawQuad(transform, color);
}

void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const Ref<Texture2D>& tex, float tilingFactor, const glm::vec4& tintColor)
{
	DrawQuad({ position.x, position.y, 0.0f }, size, tex, tilingFactor, tintColor);
}

void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const Ref<Texture2D>& tex, float tilingFactor, const glm::vec4& tintColor)
{
	glm::mat4 transform = glm::translate(glm::mat4(1.0f), position) * glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });
	DrawQuad(transform, tex, tilingFactor, tintColor);
}

void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const Ref<SubTexture2D>& subTexture, float tilingFactor, const glm::vec4& tintColor)
{
	DrawQuad({ position.x, position.y, 0.0f }, size, subTexture, tilingFactor, tintColor);
}

void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const Ref<SubTexture2D>& subTexture, float tilingFactor, const glm::vec4& tintColor)
{
	glm::mat4 transform = glm::translate(glm::mat4(1.0f), position) * glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });
	DrawQuad(transform, subTexture, tilingFactor, tintColor);
}

void Renderer2D::DrawRotatedQuad(const glm::vec2& position, const glm::vec2& size, float rotation, const glm::vec4& color)
{
	DrawRotatedQuad({ position.x, position.y, 0.0f }, size, rotation, color);
}

void Renderer2D::DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float rotation, const glm::vec4& color)
{
	glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
		* glm::rotate(glm::mat4(1.0f), rotation, { 0.0f, 0.0f, 1.0f })
		* glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });
	DrawQuad(transform, color);
}

void Renderer2D::DrawRotatedQuad(const glm::vec2& position, const glm::vec2& size, float rotation, const Ref<Texture2D>& tex, float tilingFactor, const glm::vec4& tintColor)
{
	DrawRotatedQuad({ position.x, position.y, 0.0f }, size, rotation, tex, tilingFactor, tintColor);
}

void Renderer2D::DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float rotation, const Ref<Texture2D>& tex, float tilingFactor, const glm::vec4& tintColor)
{
	glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
		* glm::rotate(glm::mat4(1.0f), rotation, { 0.0f, 0.0f, 1.0f })
		* glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

	DrawQuad(transform, tex, tilingFactor, tintColor);
}

void Renderer2D::DrawRotatedQuad(const glm::vec2& position, const glm::vec2& size, float rotation, const Ref<SubTexture2D>& subTexture, float tilingFactor, const glm::vec4& tintColor)
{
	DrawRotatedQuad({ position.x, position.y, 0.0f }, size, rotation, subTexture, tilingFactor, tintColor);
}

void Renderer2D::DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float rotation, const Ref<SubTexture2D>& subTexture, float tilingFactor, const glm::vec4& tintColor)
{
	glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
		* glm::rotate(glm::mat4(1.0f), rotation, { 0.0f, 0.0f, 1.0f })
		* glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

	DrawQuad(transform, subTexture, tilingFactor, tintColor);
}

void Renderer2D::DrawCircle(const glm::mat4& transform, const glm::vec4& color, float thickness, float fade, int entityId)
{
	WHP_PROFILE_HOT_FUNCTION();

	for (auto quadVertexPosition : s_Data.m_QuadVertexPositions)
	{
		s_Data.m_CircleVertexBufferPtr->m_WorldPosition = transform * quadVertexPosition;
		s_Data.m_CircleVertexBufferPtr->m_LocalPosition = quadVertexPosition * 2.0f;
		s_Data.m_CircleVertexBufferPtr->m_Color = color;
		s_Data.m_CircleVertexBufferPtr->m_Thickness = thickness;
		s_Data.m_CircleVertexBufferPtr->m_Fade = fade;
		s_Data.m_CircleVertexBufferPtr->m_EntityId = entityId;
		s_Data.m_CircleVertexBufferPtr++;
	}

	s_Data.m_CircleIndexCount += 6;

	s_Data.m_Stats.m_QuadCounts++;
}

void Renderer2D::DrawLine(const glm::vec3& p0, const glm::vec3& p1, const glm::vec4& color, int entityId)
{
	s_Data.m_LineVertexBufferPtr->m_Position = p0;
	s_Data.m_LineVertexBufferPtr->m_Color = color;
	s_Data.m_LineVertexBufferPtr->m_EntityId = entityId;
	s_Data.m_LineVertexBufferPtr++;

	s_Data.m_LineVertexBufferPtr->m_Position = p1;
	s_Data.m_LineVertexBufferPtr->m_Color = color;
	s_Data.m_LineVertexBufferPtr->m_EntityId = entityId;
	s_Data.m_LineVertexBufferPtr++;

	s_Data.m_LineVertexCount += 2;
}

void Renderer2D::DrawRect(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color, int entityId)
{
	glm::vec3 p0 = glm::vec3(position.x - size.x * 0.5f, position.y - size.y * 0.5f, position.z);
	glm::vec3 p1 = glm::vec3(position.x + size.x * 0.5f, position.y - size.y * 0.5f, position.z);
	glm::vec3 p2 = glm::vec3(position.x + size.x * 0.5f, position.y + size.y * 0.5f, position.z);
	glm::vec3 p3 = glm::vec3(position.x - size.x * 0.5f, position.y + size.y * 0.5f, position.z);

	DrawLine(p0, p1, color, entityId);
	DrawLine(p1, p2, color, entityId);
	DrawLine(p2, p3, color, entityId);
	DrawLine(p3, p0, color, entityId);
}

void Renderer2D::DrawRect(const glm::mat4& transform, const glm::vec4& color, int entityId)
{
	glm::vec3 lineVertices[4];
	for (size_t i = 0; i < 4; i++)
		lineVertices[i] = transform * s_Data.m_QuadVertexPositions[i];

	DrawLine(lineVertices[0], lineVertices[1], color, entityId);
	DrawLine(lineVertices[1], lineVertices[2], color, entityId);
	DrawLine(lineVertices[2], lineVertices[3], color, entityId);
	DrawLine(lineVertices[3], lineVertices[0], color, entityId);
}

void Renderer2D::DrawSprite(const glm::mat4& transform, const SpriteRendererComponent& src, int entityId)
{
	if (src.m_Texture)
	{
		Ref<Texture2D> texture = GetFrameCachedTexture(src.m_Texture);
		if (Ref<SubTexture2D> subTexture = CreateSubTextureFromSpriteIndex(texture, src.m_Texture, src.m_TextureSpriteIndex))
			DrawQuad(transform, subTexture, src.m_TilingFactor, src.m_Color, entityId);
		else if (texture)
			DrawQuad(transform, texture, src.m_TilingFactor, src.m_Color, entityId);
		else
			DrawQuad(transform, src.m_Color, entityId);
	}
	else
	{
		DrawQuad(transform, src.m_Color, entityId);
	}
}

void Renderer2D::DrawString(const std::string& text, Ref<Font> font, const glm::mat4& transform, const TextParams& params, int entityId)
{
	WHP_PROFILE_HOT_FUNCTION();
	if (!font)
	{
		WHP_CORE_ERROR("[Renderer2D] Null Font!");
		return;
	}
	const auto& fontGeometry = font->GetMsdfData()->m_FontGeometry;
	const auto& metrics = fontGeometry.getMetrics();
	Ref<Texture2D> fontAtlas = font->GetAtlasTexture();
	if (!fontAtlas)
	{
		WHP_CORE_ERROR("[Renderer2D] Font has no atlas Texture!");
		return;
	}

	if (s_Data.m_TextIndexCount && s_Data.m_FontAtlasTexture && s_Data.m_FontAtlasTexture != fontAtlas)
		NextBatch();
	s_Data.m_FontAtlasTexture = fontAtlas;

	double x = 0.0;
	double fsScale = 1.0 / (metrics.ascenderY - metrics.descenderY);
	double y = 0.0;

	const auto spaceGlyph = fontGeometry.getGlyph(' ');
	const float spaceGlyphAdvance = spaceGlyph ? static_cast<float>(spaceGlyph->getAdvance()) : 1.0f;

	for (size_t i = 0; i < text.size(); i++)
	{
		char character = text[i];
		if (character == '\r')
			continue;

		if (character == '\n')
		{
			x = 0;
			y -= fsScale * metrics.lineHeight + params.m_LineSpacing;
			continue;
		}

		if (character == ' ')
		{
			float advance = spaceGlyphAdvance;
			if (i < text.size() - 1)
			{
				char nextCharacter = text[i + 1];
				double glyphAdvance;
				fontGeometry.getAdvance(glyphAdvance, character, nextCharacter);
				advance = static_cast<float>(glyphAdvance);
			}

			x += fsScale * advance + params.m_Kerning;
			continue;
		}

		if (character == '\t')
		{
			// is this right?
			x += 4.0f * (fsScale * spaceGlyphAdvance + params.m_Kerning);
			continue;
		}
		auto glyph = fontGeometry.getGlyph(character);
		if (!glyph)
			glyph = fontGeometry.getGlyph('?');
		if (!glyph)
			continue;

		if (s_Data.m_TextIndexCount >= Renderer2DData::MaxIndices)
			NextBatch();

		double al, ab, ar, at;
		glyph->getQuadAtlasBounds(al, ab, ar, at);
		glm::vec2 textureCoordMin(static_cast<float>(al), static_cast<float>(ab));
		glm::vec2 textureCoordMax(static_cast<float>(ar), static_cast<float>(at));

		double pl, pb, pr, pt;
		glyph->getQuadPlaneBounds(pl, pb, pr, pt);
		glm::vec2 quadMin(static_cast<float>(pl), static_cast<float>(pb));
		glm::vec2 quadMax(static_cast<float>(pr), static_cast<float>(pt));

		quadMin *= fsScale;
		quadMax *= fsScale;
		quadMin += glm::vec2(x, y);
		quadMax += glm::vec2(x, y);

		float texelWidth = 1.0f / static_cast<float>(fontAtlas->GetWidth());
		float texelHeight = 1.0f / static_cast<float>(fontAtlas->GetHeight());
		textureCoordMin *= glm::vec2(texelWidth, texelHeight);
		textureCoordMax *= glm::vec2(texelWidth, texelHeight);

		s_Data.m_TextVertexBufferPtr->m_Position = transform * glm::vec4(quadMin, 0.0f, 1.0f);
		s_Data.m_TextVertexBufferPtr->m_Color = params.m_Color;
		s_Data.m_TextVertexBufferPtr->m_TextureCoord = textureCoordMin;
		s_Data.m_TextVertexBufferPtr->m_EntityId = entityId;
		s_Data.m_TextVertexBufferPtr++;

		s_Data.m_TextVertexBufferPtr->m_Position = transform * glm::vec4(quadMin.x, quadMax.y, 0.0f, 1.0f);
		s_Data.m_TextVertexBufferPtr->m_Color = params.m_Color;
		s_Data.m_TextVertexBufferPtr->m_TextureCoord = { textureCoordMin.x, textureCoordMax.y };
		s_Data.m_TextVertexBufferPtr->m_EntityId = entityId;
		s_Data.m_TextVertexBufferPtr++;

		s_Data.m_TextVertexBufferPtr->m_Position = transform * glm::vec4(quadMax, 0.0f, 1.0f);
		s_Data.m_TextVertexBufferPtr->m_Color = params.m_Color;
		s_Data.m_TextVertexBufferPtr->m_TextureCoord = textureCoordMax;
		s_Data.m_TextVertexBufferPtr->m_EntityId = entityId;
		s_Data.m_TextVertexBufferPtr++;

		s_Data.m_TextVertexBufferPtr->m_Position = transform * glm::vec4(quadMax.x, quadMin.y, 0.0f, 1.0f);
		s_Data.m_TextVertexBufferPtr->m_Color = params.m_Color;
		s_Data.m_TextVertexBufferPtr->m_TextureCoord = { textureCoordMax.x, textureCoordMin.y };
		s_Data.m_TextVertexBufferPtr->m_EntityId = entityId;
		s_Data.m_TextVertexBufferPtr++;

		s_Data.m_TextIndexCount += 6;
		s_Data.m_Stats.m_QuadCounts++;

		if (i < text.size() - 1)
		{
			double advance = glyph->getAdvance();
			char nextCharacter = text[i + 1];
			fontGeometry.getAdvance(advance, character, nextCharacter);

			x += fsScale * advance + params.m_Kerning;
		}
	}
}

void Renderer2D::DrawString(const std::string& text, const glm::mat4& transform, const TextComponent& component, int entityId)
{
	DrawString(text,
		component.m_Font ? std::static_pointer_cast<Font>(Project::GetActive()->GetAssetManager()->GetAsset(component.m_Font)) : Font::GetDefault(),
		transform,
		{
			.m_Color = component.m_Color,
			.m_Kerning = component.m_Kerning,
			.m_LineSpacing = component.m_LineSpacing
		},
		entityId);
}

void Renderer2D::SetLineWidth(float width)
{
	RenderCommand::SetLineWidth(width);
}

void Renderer2D::ResetStats()
{
	memset(&s_Data.m_Stats, 0, sizeof(Renderer2D::Statistics));
}

Renderer2D::Statistics Renderer2D::GetStats()
{
	return s_Data.m_Stats;
}

void Renderer2D::StartBatch()
{
	s_Data.m_QuadIndexCount = 0;
	s_Data.m_QuadVertexBufferPtr = s_Data.m_QuadVertexBufferBase;

	s_Data.m_CircleIndexCount = 0;
	s_Data.m_CircleVertexBufferPtr = s_Data.m_CircleVertexBufferBase;

	s_Data.m_LineVertexCount = 0;
	s_Data.m_LineVertexBufferPtr = s_Data.m_LineVertexBufferBase;

	s_Data.m_TextIndexCount = 0;
	s_Data.m_TextVertexBufferPtr = s_Data.m_TextVertexBufferBase;

	s_Data.m_TextureSlotIndex = 1;
}

void Renderer2D::NextBatch()
{
	Flush();
	StartBatch();
}

_WHIP_END
