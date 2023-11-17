#include <whippch.h>
#include "OpenGLRenderAPI.h"

#include <glad/glad.h>

_WHIP_START

void opengl_render_API::init()
{
	WHP_PROFILE_FUNCTION();

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_DEPTH_TEST);
}

void opengl_render_API::set_viewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
	glViewport(x, y, width, height);
}

void opengl_render_API::set_clear_color(const glm::vec4& color)
{
	glClearColor(color.r, color.g, color.b, color.a);
}

void opengl_render_API::clear()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void opengl_render_API::draw_indexed(const ref<vertex_array>& vertexArray, uint32_t index_count)
{
	uint32_t count = (index_count != 0) ? index_count : vertexArray->get_index_buffer()->get_count();
	glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, nullptr);
}


_WHIP_END
