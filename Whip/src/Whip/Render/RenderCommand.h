#pragma once

#include "RenderAPI.h"

_WHIP_START

class render_command
{
private:
	static render_API* s_render_API;
public:
	inline static void init()
	{
		s_render_API->init();
	}

	inline static void set_viewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
	{
		s_render_API->set_viewport(x, y, width, height);
	}

	inline static void set_clear_color(const glm::vec4& color)
	{
		s_render_API->set_clear_color(color);
	}
	inline static void clear()
	{
		s_render_API->clear();
	}
	inline static void draw_indexed(const whip::ref<vertex_array>& vertexArray, uint32_t index_count = 0)
	{
		s_render_API->draw_indexed(vertexArray, index_count);
	}
};

_WHIP_END