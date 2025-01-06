#include <whippch.h>

#include <glad/glad.h>

#include "OpenGLVertexArray.h"

_WHIP_START

static GLenum shader_data_type_to_opengl_base_type(shader_data_type type)
{
	switch (type)
	{
		case whip::shader_data_type::none:			WHP_CORE_ASSERT(false, "ShaderDataType is None!"); return 0;
		case whip::shader_data_type::Float:			return GL_FLOAT;
		case whip::shader_data_type::Float2:		return GL_FLOAT;
		case whip::shader_data_type::Float3:		return GL_FLOAT;
		case whip::shader_data_type::Float4:		return GL_FLOAT;
		case whip::shader_data_type::Mat3:			return GL_FLOAT;
		case whip::shader_data_type::Mat4:			return GL_FLOAT;
		case whip::shader_data_type::Bool:			return GL_BOOL;
		case whip::shader_data_type::Int:			return GL_INT;
		case whip::shader_data_type::Int2:			return GL_INT;
		case whip::shader_data_type::Int3:			return GL_INT;
		case whip::shader_data_type::Int4:			return GL_INT;
	}
	WHP_CORE_ASSERT(false, "Unknown ShaderDataType!");
	return 0;
}

opengl_vertex_array::opengl_vertex_array()
{
	WHP_PROFILE_FUNCTION();

	glCreateVertexArrays(1, &m_rendererID);
}

opengl_vertex_array::~opengl_vertex_array()
{
	glDeleteVertexArrays(1, &m_rendererID);
}

void opengl_vertex_array::bind() const
{
	WHP_PROFILE_FUNCTION();

	glBindVertexArray(m_rendererID);
}

void opengl_vertex_array::unbind() const
{
	WHP_PROFILE_FUNCTION();

	glBindVertexArray(0);
}

void opengl_vertex_array::add_vertex_buffer(const ref<vertex_buffer>& vertex_buffer_in)
{
	WHP_PROFILE_FUNCTION();

	WHP_CORE_ASSERT(vertex_buffer_in->get_layout().get_elements().size(), "Vertex Buffer has no layout");
	
	glBindVertexArray(m_rendererID);
	vertex_buffer_in->bind();

	uint32_t index = 0;
	const auto& layout = vertex_buffer_in->get_layout();
	for (const auto& elem : layout)
	{
		switch(elem.type)
		{
		case shader_data_type::Float:
		case shader_data_type::Float2:
		case shader_data_type::Float3:
		case shader_data_type::Float4:
		{
			glEnableVertexAttribArray(m_vertex_buffer_index);
			glVertexAttribPointer(m_vertex_buffer_index, elem.get_component_count(), shader_data_type_to_opengl_base_type(elem.type), elem.normalized ? GL_TRUE : GL_FALSE, static_cast<GLsizei>(layout.get_stride()), (const void*)elem.offset);
			m_vertex_buffer_index++;
			break;
		}
		case shader_data_type::Int:
		case shader_data_type::Int2:
		case shader_data_type::Int3:
		case shader_data_type::Int4:
		case shader_data_type::Bool:
		{
			glEnableVertexAttribArray(m_vertex_buffer_index);
			glVertexAttribIPointer(m_vertex_buffer_index, elem.get_component_count(), shader_data_type_to_opengl_base_type(elem.type), static_cast<GLsizei>(layout.get_stride()), (const void*)elem.offset);
			m_vertex_buffer_index++;
			break;
		}
		case shader_data_type::Mat3:
		case shader_data_type::Mat4:
		{
			uint8_t count = elem.get_component_count();
			for (uint8_t i = 0; i < count; i++)
			{
				glEnableVertexAttribArray(m_vertex_buffer_index);
				glVertexAttribPointer(m_vertex_buffer_index, count, shader_data_type_to_opengl_base_type(elem.type), elem.normalized ? GL_TRUE : GL_FALSE, static_cast<GLsizei>(layout.get_stride()), (const void*)(elem.offset + sizeof(float) * count * i));
				glVertexAttribDivisor(m_vertex_buffer_index, 1);
				m_vertex_buffer_index++;
			}
			break;
		}
		default:
			WHP_CORE_ASSERT(false, "Unknown shader_data_type!");
		}
	}

	m_vertex_buffers.push_back(vertex_buffer_in);
}

void opengl_vertex_array::set_index_buffer(const ref<index_buffer>& indexBuffer)
{
	WHP_PROFILE_FUNCTION();

	glBindVertexArray(m_rendererID);
	indexBuffer->bind();
	m_index_buffers = indexBuffer;
}


_WHIP_END
