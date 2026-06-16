#include "WhipPch.h"
#include <Whip/Render/Font.h>
#include <Whip/Core/Application.h>

#ifdef INFINITE
#undef INFINITE
#endif // INFINITE

#pragma warning(push)
#pragma warning(disable : 4244 4267)
#include <msdf-atlas-gen.h>
#pragma warning(pop)

#include <Whip/Render/MsdfData.h>

#include <coco.h>

#define DEFAULT_ANGLE_THRESHOLD 3.0
#define LCG_MULTIPLIER 6364136223846793005ull
#define LCG_INCREMENT 1442695040888963407ull
#define THREAD_COUNT 4

_WHIP_START

namespace Utils
{
	template<typename T, typename S, int N, msdf_atlas::GeneratorFunction<S, N> GenFunc>
	static Ref<Texture2D> CreateAndCacheAtlas(const std::string& fontName, float fontSize, const std::vector<msdf_atlas::GlyphGeometry>& glyphs,
		const msdf_atlas::FontGeometry& fontGeometry, uint32_t width, uint32_t height)
	{
		msdf_atlas::GeneratorAttributes attributes;
		attributes.config.overlapSupport = true;
		attributes.scanlinePass = true;

		msdf_atlas::ImmediateAtlasGenerator<S, N, GenFunc, msdf_atlas::BitmapAtlasStorage<T, N>> generator(width, height);
		generator.setAttributes(attributes);
		generator.setThreadCount(THREAD_COUNT);
		generator.generate(glyphs.data(), (int)glyphs.size());

		msdfgen::BitmapConstRef<T, N> bitmap = (msdfgen::BitmapConstRef<T, N>)generator.atlasStorage();

		TextureSpecification spec;
		spec.m_Width = bitmap.width;
		spec.m_Height = bitmap.height;
		spec.m_Format = ImageFormat::Rgb8;
		spec.m_GenerateMips = false;

		Ref<Texture2D> texture = Texture2D::Create(spec);
		texture->SetData(RawBuffer((void*)bitmap.pixels, bitmap.width * bitmap.height * 3));
		return texture;
	}
}

Font::Font(const std::filesystem::path& filepath, AssetHandle handle) : Asset(handle), m_Data(new MsdfData())
{
	msdfgen::FreetypeHandle* ft = msdfgen::initializeFreetype();
	WHP_CORE_ASSERT(ft);

	std::string fileString = filepath.string();

	msdfgen::FontHandle* font = msdfgen::loadFont(ft, fileString.c_str());
	if (!font)
	{
		WHP_CORE_ERROR("[Font Engine] Failed to load Font: {}", fileString);
		return;
	}

	struct CharsetRange
	{
		uint32_t begin, end;
	};

	// From imgui_draw.cpp
	static const CharsetRange charsetRanges[] =
	{
		{ 0x0020, 0x00FF }
	};

	msdf_atlas::Charset charset;
	for (CharsetRange range : charsetRanges)
	{
		for (uint32_t c = range.begin; c <= range.end; c++)
			charset.add(c);
	}

	double fontScale = 1.0;
	m_Data->m_FontGeometry = msdf_atlas::FontGeometry(&m_Data->m_Glyphs);
	int glyphsLoaded = m_Data->m_FontGeometry.loadCharset(font, fontScale, charset);
	WHP_CORE_INFO("[Font Engine] Loaded {} glyphs from Font (out of {})", glyphsLoaded, charset.size());

	msdfgen::FontMetrics fontMetrics;
	msdfgen::getFontMetrics(fontMetrics, font);

	msdf_atlas::TightAtlasPacker atlasPacker;

	atlasPacker.setPixelRange(2.0);
	atlasPacker.setMiterLimit(1.0);
	atlasPacker.setScale(fontMetrics.emSize);
	int remaining = atlasPacker.pack(m_Data->m_Glyphs.data(), (int)m_Data->m_Glyphs.size());
	WHP_CORE_ASSERT(remaining == 0);

	int width, height;
	atlasPacker.getDimensions(width, height);
	double emSize = atlasPacker.getScale();

	uint64_t coloringSeed = 0;

	bool expensiveColoring = true;
	if (expensiveColoring)
	{
		msdf_atlas::Workload([&glyphs = m_Data->m_Glyphs, &coloringSeed](int i, int threadNo) -> bool
			{
			unsigned long long glyphSeed = (LCG_MULTIPLIER * (coloringSeed ^ i) + LCG_INCREMENT) * !!coloringSeed;
			glyphs[i].edgeColoring(msdfgen::edgeColoringInkTrap, DEFAULT_ANGLE_THRESHOLD, glyphSeed);
			return true;
			}
		, (int)m_Data->m_Glyphs.size()).finish(THREAD_COUNT);
	}
	else {
		unsigned long long glyphSeed = coloringSeed;
		for (msdf_atlas::GlyphGeometry& glyph : m_Data->m_Glyphs)
		{
			glyphSeed *= LCG_MULTIPLIER;
			glyph.edgeColoring(msdfgen::edgeColoringInkTrap, DEFAULT_ANGLE_THRESHOLD, glyphSeed);
		}
	}
	m_AtlasTexture = Utils::CreateAndCacheAtlas<uint8_t, float, 3, msdf_atlas::msdfGenerator>("Test", (float)emSize, m_Data->m_Glyphs, m_Data->m_FontGeometry, width, height);

	WHP_CORE_INFO("[Font Engine] Loaded atlas Texture from Font '{}'", filepath.string());

	msdfgen::destroyFont(font);
	msdfgen::deinitializeFreetype(ft);
}

Font::~Font()
{
	delete m_Data;
}

Ref<Font> Font::GetDefault()
{
	static Ref<Font> defaultFont;
	if (!defaultFont)
		defaultFont = MakeRef<Font>("assets/fonts/opensans/OpenSans-Regular.ttf");

	return defaultFont;
}

_WHIP_END
