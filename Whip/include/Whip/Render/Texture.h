#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/Memory.h>
#include <Whip/Helper/Buffer.h>
#include <Whip/Asset/Asset.h>

#include <string>

_WHIP_START

enum class ImageFormat
{
	None = 0, 
	R8,
	Rgb8,
	Rgba8,
	Rgba32F
};

enum class TextureFilterMode
{
	Nearest = 0,
	Linear
};

enum class TextureWrapMode
{
	Repeat = 0,
	ClampToEdge
};

struct TextureSpecification
{
	uint32_t m_Width = 1;
	uint32_t m_Height = 1;
	ImageFormat m_Format = ImageFormat::Rgba8;
	bool m_GenerateMips = true;
	TextureFilterMode m_FilterMode = TextureFilterMode::Linear;
	TextureWrapMode m_WrapMode = TextureWrapMode::Repeat;
};

class Texture : public Asset
{
public:
	virtual ~Texture() = default;

	virtual const TextureSpecification& GetSpecification() const = 0;

	virtual uint32_t GetWidth() const = 0;
	virtual uint32_t GetHeight() const = 0;

	virtual RendererId GetRendererId() const = 0;

	virtual void SetData(RawBuffer data) = 0;
	virtual RawBuffer GetData() const = 0;
	virtual void Bind(uint32_t slot = 0) const = 0;
	virtual bool IsLoaded() const = 0;
	virtual bool operator==(const Texture& other) const = 0;
};

typedef int FlipDirection;

enum : FlipDirection
{
	FlipDirectionNone			= WHP_BIT(0),
	FlipDirectionHorizontal	= WHP_BIT(1),
	FlipDirectionVertical		= WHP_BIT(2)
};

class Texture2D : public Texture
{
public:
	static Ref<Texture2D> Create(const TextureSpecification& specification, RawBuffer data = RawBuffer());

	static Ref<Texture2D> CreateFromCoords(const Ref<Texture2D>& atlas, const glm::vec2& coords, const glm::vec2& cellSize, 
		const glm::vec2& pixelSizeBetweenSprites = { 0.0f, 0.0f }, const glm::vec2& spriteSize = { 1.0f, 1.0f });
	static void FlipTextureBuffer(RawBuffer& buffer, int width, int height, int channels, FlipDirection direction);
	static AssetType GetStaticType() { return AssetType::Texture2D; }
	virtual AssetType GetType() const override { return GetStaticType(); }
};

_WHIP_END
