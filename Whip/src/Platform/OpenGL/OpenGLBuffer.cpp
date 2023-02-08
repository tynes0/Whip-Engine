#include "whippch.h"
#include "OpenGLBuffer.h"

#include <glad/glad.h>

_WHIP_START

// VERTEX BUFFER

opengl_vertex_buffer::opengl_vertex_buffer(float* vertices, uint32_t size)
{
	glCreateBuffers(1, &m_rendererID);
	glBindBuffer(GL_ARRAY_BUFFER, m_rendererID);
	glBufferData(GL_ARRAY_BUFFER, size, vertices, GL_STATIC_DRAW);
}

opengl_vertex_buffer::~opengl_vertex_buffer()
{
	glDeleteBuffers(1, &m_rendererID);
}

void opengl_vertex_buffer::bind() const
{
	glBindBuffer(GL_ARRAY_BUFFER, m_rendererID);
}

void opengl_vertex_buffer::unbind() const
{
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// INDEX BUFFER

opengl_index_buffer::opengl_index_buffer(uint32_t* indices, uint32_t count)
	: m_Count(count)
{
	glCreateBuffers(1, &m_rendererID);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_rendererID);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, count * sizeof(uint32_t), indices, GL_STATIC_DRAW);
}

opengl_index_buffer::~opengl_index_buffer()
{
	glDeleteBuffers(1, &m_rendererID);
}

void opengl_index_buffer::bind() const
{
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_rendererID);
}

void opengl_index_buffer::unbind() const
{
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

_WHIP_END