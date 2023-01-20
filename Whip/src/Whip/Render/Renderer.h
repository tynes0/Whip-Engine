#pragma once

#include <Whip/Core.h>

_WHIP_START

enum class RendererAPI
{
	None = 0,
	OpenGL = 1
	// for now 
};

class Renderer
{
private:
	static RendererAPI s_RendererAPI;
public:
	inline static RendererAPI GetRendererAPI() { return s_RendererAPI; }
};

_WHIP_END