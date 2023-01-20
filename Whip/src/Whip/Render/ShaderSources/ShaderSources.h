#pragma once
#include <Whip/Core.h>
#include <string>


_WHIP_START

class ShaderSources
{
private:
	static const std::string VertexSrc;
	static const std::string FragmentSrc;
public:
	static const std::string& GetVertexSrc() { return VertexSrc; }
	static const std::string& GetFragmentSrc() { return FragmentSrc; }
};

#define WHP_VERTEX_SRC ::Whip::ShaderSources::GetVertexSrc()
#define WHP_FRAGMENT_SRC ::Whip::ShaderSources::GetFragmentSrc()

_WHIP_END