#include "whippch.h"
#include "font.h"
#include "Whip/Core/Application.h"

#ifdef INFINITE
#undef INFINITE
#endif // INFINITE

#pragma warning(push)
#pragma warning(disable : 4244 4267)
#include <msdf-atlas-gen.h>
#pragma warning(pop)

#include "msdf_data.h"

#include <coco.h>

#define DEFAULT_ANGLE_THRESHOLD 3.0
#define LCG_MULTIPLIER 6364136223846793005ull
#define LCG_INCREMENT 1442695040888963407ull
#define THREAD_COUNT 4

_WHIP_START

namespace utils
{
	template<typename T, typename S, int N, msdf_atlas::GeneratorFunction<S, N> GenFunc>
	static ref<texture2D> create_and_cache_atlas(const std::string& fontName, float fontSize, const std::vector<msdf_atlas::GlyphGeometry>& glyphs,
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

		texture_specification spec;
		spec.width = bitmap.width;
		spec.height = bitmap.height;
		spec.format = image_format::RGB8;
		spec.generate_mips = false;

		ref<texture2D> texture = texture2D::create(spec);
		texture->set_data(raw_buffer((void*)bitmap.pixels, bitmap.width * bitmap.height * 3));
		return texture;
	}
}

font::font(const std::filesystem::path& filepath, asset_handle handle) : asset(handle), m_data(new msdf_data())
{
	msdfgen::FreetypeHandle* ft = msdfgen::initializeFreetype();
	WHP_CORE_ASSERT(ft);

	std::string fileString = filepath.string();

	msdfgen::FontHandle* font = msdfgen::loadFont(ft, fileString.c_str());
	if (!font)
	{
		WHP_CORE_ERROR("[Font Engine] Failed to load font: {}", fileString);
		return;
	}

	struct charset_range
	{
		uint32_t begin, end;
	};

	// From imgui_draw.cpp
	static const charset_range charset_ranges[] =
	{
		{ 0x0020, 0x00FF }
	};

	msdf_atlas::Charset charset;
	for (charset_range range : charset_ranges)
	{
		for (uint32_t c = range.begin; c <= range.end; c++)
			charset.add(c);
	}

	double font_scale = 1.0;
	m_data->font_geometry = msdf_atlas::FontGeometry(&m_data->glyphs);
	int glyphs_loaded = m_data->font_geometry.loadCharset(font, font_scale, charset);
	WHP_CORE_INFO("[Font Engine] Loaded {} glyphs from font (out of {})", glyphs_loaded, charset.size());


	double em_size = 40.0;

	msdf_atlas::TightAtlasPacker atlas_packer;

	atlas_packer.setPixelRange(2.0);
	atlas_packer.setMiterLimit(1.0);
	atlas_packer.setPadding(0);
	atlas_packer.setScale(em_size);
	int remaining = atlas_packer.pack(m_data->glyphs.data(), (int)m_data->glyphs.size());
	WHP_CORE_ASSERT(remaining == 0);

	int width, height;
	atlas_packer.getDimensions(width, height);
	em_size = atlas_packer.getScale();

	uint64_t coloring_seed = 0;

	bool expensive_coloring = true;
	if (expensive_coloring)
	{
		msdf_atlas::Workload([&glyphs = m_data->glyphs, &coloring_seed](int i, int threadNo) -> bool 
			{
			unsigned long long glyphSeed = (LCG_MULTIPLIER * (coloring_seed ^ i) + LCG_INCREMENT) * !!coloring_seed;
			glyphs[i].edgeColoring(msdfgen::edgeColoringInkTrap, DEFAULT_ANGLE_THRESHOLD, glyphSeed);
			return true;
			}
		, (int)m_data->glyphs.size()).finish(THREAD_COUNT);
	}
	else {
		unsigned long long glyph_seed = coloring_seed;
		for (msdf_atlas::GlyphGeometry& glyph : m_data->glyphs)
		{
			glyph_seed *= LCG_MULTIPLIER;
			glyph.edgeColoring(msdfgen::edgeColoringInkTrap, DEFAULT_ANGLE_THRESHOLD, glyph_seed);
		}
	}
	m_atlas_texture = utils::create_and_cache_atlas<uint8_t, float, 3, msdf_atlas::msdfGenerator>("Test", (float)em_size, m_data->glyphs, m_data->font_geometry, width, height);

	WHP_CORE_INFO("[Font Engine] Loaded atlas texture from font '{}'", filepath.string());

	msdfgen::destroyFont(font);
	msdfgen::deinitializeFreetype(ft);
}

font::~font()
{
	delete m_data;
}

ref<font> font::get_default()
{
	static ref<font> default_font;
	if (!default_font)
		default_font = make_ref<font>("assets/fonts/opensans/OpenSans-Regular.ttf");

	return default_font;
}

_WHIP_END
