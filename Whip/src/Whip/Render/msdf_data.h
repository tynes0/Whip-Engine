#pragma once

#include <Whip/Core/Core.h>
#ifdef INFINITE
#undef INFINITE
#endif // INFINITE
#include <msdf-atlas-gen.h>

#include <vector>

_WHIP_START

struct msdf_data
{
	std::vector<msdf_atlas::GlyphGeometry> glyphs;
	msdf_atlas::FontGeometry font_geometry;
};


_WHIP_END
