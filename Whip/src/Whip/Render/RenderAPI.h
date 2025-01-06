#pragma once

#include <Whip/Render/VertexArray.h>

#include <glm/glm.hpp>

_WHIP_START

class render_API
{
public:
	enum class API
	{
		none = 0,
		opengl = 1
		// for now 
	};
public:
	virtual ~render_API() = default;

	virtual void init() = 0;
	virtual void set_viewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) = 0;
	virtual void set_clear_color(const glm::vec4& color) = 0;
	virtual void clear() = 0;
	virtual void draw_indexed(const whip::ref<vertex_array>& vertexArray, uint32_t index_count = 0) = 0;
	virtual void draw_lines(const ref<vertex_array>& vertexArray, uint32_t vertex_count) = 0;
	virtual void set_line_width(float width) = 0;
	WHP_NODISCARD inline static API get_API() { return s_API; }
private:
	static API s_API;
};

_WHIP_END
