#pragma once
#include <Whip/Core.h>
#include <string>


_WHIP_START

class ShaderSources
{
private:
	static const std::string VertexSrc;
	static const std::string FragmentSrc;

	static const std::string SecondVertexSrc;
	static const std::string SecondFragmentSrc;
public:
	WHP_NODISCARD inline static const std::string& GetVertexSrc() { return VertexSrc; }
	WHP_NODISCARD inline static const std::string& GetSecondVertexSrc() { return SecondVertexSrc; }
	WHP_NODISCARD inline static const std::string& GetFragmentSrc() { return FragmentSrc; }
	WHP_NODISCARD inline static const std::string& GetSecondFragmentSrc() { return SecondFragmentSrc; }
};

#define WHP_VERTEX_SRC					::Whip::ShaderSources::GetVertexSrc()
#define WHP_FRAGMENT_SRC				::Whip::ShaderSources::GetFragmentSrc()
#define WHP_SECOND_VERTEX_SRC			::Whip::ShaderSources::GetSecondVertexSrc()
#define WHP_SECOND_FRAGMENT_SRC			::Whip::ShaderSources::GetSecondFragmentSrc()

_WHIP_END