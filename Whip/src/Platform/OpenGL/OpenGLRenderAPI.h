#pragma once

#include <Whip/Render/RenderAPI.h>

_WHIP_START

class opengl_render_API : public render_API
{
	virtual void init() override;
	virtual void set_viewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;
	virtual void set_clear_color(const glm::vec4& color) override;
	virtual void clear() override;
	virtual void draw_indexed(const ref<vertex_array>& vertexArray, uint32_t index_count = 0) override;
	virtual void draw_lines(const ref<vertex_array>& vertexArray, uint32_t vertex_count) override;
	virtual void set_line_width(float width) override;
};

_WHIP_END
