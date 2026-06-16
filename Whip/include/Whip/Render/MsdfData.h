#pragma once

#include <Whip/Core/Core.h>
#ifdef INFINITE
#undef INFINITE
#endif // INFINITE
#include <msdf-atlas-gen.h>

#include <vector>

_WHIP_START

struct MsdfData
{
	std::vector<msdf_atlas::GlyphGeometry> m_Glyphs;
	msdf_atlas::FontGeometry m_FontGeometry;
};


_WHIP_END
