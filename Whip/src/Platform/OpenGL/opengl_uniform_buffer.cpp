#include "whippch.h"
#include "opengl_uniform_buffer.h"

#include <glad/glad.h>

_WHIP_START

opengl_uniform_buffer::opengl_uniform_buffer(uint32_t size, uint32_t binding)
{
	glCreateBuffers(1, &m_rendererID);
	glNamedBufferData(m_rendererID, size, nullptr, GL_DYNAMIC_DRAW);
	glBindBufferBase(GL_UNIFORM_BUFFER, binding, m_rendererID);
}

opengl_uniform_buffer::~opengl_uniform_buffer()
{
	glDeleteBuffers(1, &m_rendererID);
}

void opengl_uniform_buffer::set_data(const void* data, uint32_t size, uint32_t offset)
{
	glNamedBufferSubData(m_rendererID, offset, size, data);
}

_WHIP_END
