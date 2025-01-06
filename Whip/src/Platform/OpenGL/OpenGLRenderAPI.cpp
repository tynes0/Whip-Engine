#include <whippch.h>
#include "OpenGLRenderAPI.h"

#include <glad/glad.h>

_WHIP_START

void opengl_message_callback(unsigned source, unsigned type, unsigned id, unsigned severity, int length, const char* message, const void* userParam)
{
	switch (severity)
	{
	case GL_DEBUG_SEVERITY_HIGH:         WHP_CORE_CRITICAL(message); return;
	case GL_DEBUG_SEVERITY_MEDIUM:       WHP_CORE_ERROR(message); return;
	case GL_DEBUG_SEVERITY_LOW:          WHP_CORE_WARN(message); return;
	case GL_DEBUG_SEVERITY_NOTIFICATION: WHP_CORE_TRACE(message); return;
	}

	WHP_CORE_ASSERT(false, "Unknown severity level!");
}

void opengl_render_API::init()
{
	WHP_PROFILE_FUNCTION();

#ifdef WHP_DEBUG
	glEnable(GL_DEBUG_OUTPUT);
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	glDebugMessageCallback(opengl_message_callback, nullptr);

	glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, NULL, GL_FALSE);
#endif

	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0.0f);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_LINE_SMOOTH);
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
	vertexArray->bind();
	uint32_t count = (index_count != 0) ? index_count : vertexArray->get_index_buffer()->get_count();
	glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, nullptr);
}

void opengl_render_API::draw_lines(const ref<vertex_array>& vertexArray, uint32_t vertex_count)
{
	vertexArray->bind();
	glDrawArrays(GL_LINES, 0, vertex_count);
}

void opengl_render_API::set_line_width(float width)
{
	if (width < 0.1f)
		width = 0.1f;
	else if (width > 1.0f)
		width = 1.0f;

	glLineWidth(width);
}

_WHIP_END
