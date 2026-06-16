#include <WhipPch.h>

#include <glad/glad.h>

#include "OpenGLVertexArray.h"

_WHIP_START

static GLenum ShaderDataTypeToOpenGLBaseType(ShaderDataType type)
{
	switch (type)
	{
		case whip::ShaderDataType::None:			WHP_CORE_ASSERT(false, "ShaderDataType is None!"); return 0;
		case whip::ShaderDataType::Float:			return GL_FLOAT;
		case whip::ShaderDataType::Float2:		return GL_FLOAT;
		case whip::ShaderDataType::Float3:		return GL_FLOAT;
		case whip::ShaderDataType::Float4:		return GL_FLOAT;
		case whip::ShaderDataType::Mat3:			return GL_FLOAT;
		case whip::ShaderDataType::Mat4:			return GL_FLOAT;
		case whip::ShaderDataType::Bool:			return GL_BOOL;
		case whip::ShaderDataType::Int:			return GL_INT;
		case whip::ShaderDataType::Int2:			return GL_INT;
		case whip::ShaderDataType::Int3:			return GL_INT;
		case whip::ShaderDataType::Int4:			return GL_INT;
	}
	WHP_CORE_ASSERT(false, "Unknown ShaderDataType!");
	return 0;
}

OpenGLVertexArray::OpenGLVertexArray()
{
	WHP_PROFILE_FUNCTION();

	glCreateVertexArrays(1, &m_RendererID);
}

OpenGLVertexArray::~OpenGLVertexArray()
{
	glDeleteVertexArrays(1, &m_RendererID);
}

void OpenGLVertexArray::Bind() const
{
	WHP_PROFILE_FUNCTION();

	glBindVertexArray(m_RendererID);
}

void OpenGLVertexArray::Unbind() const
{
	WHP_PROFILE_FUNCTION();

	glBindVertexArray(0);
}

void OpenGLVertexArray::AddVertexBuffer(const Ref<VertexBuffer>& vertexBuffer)
{
	WHP_PROFILE_FUNCTION();

	WHP_CORE_ASSERT(vertexBuffer->GetLayout().GetElements().size(), "Vertex Buffer has no layout");
	
	glBindVertexArray(m_RendererID);
	vertexBuffer->Bind();

	uint32_t index = 0;
	const auto& layout = vertexBuffer->GetLayout();
	for (const auto& elem : layout)
	{
		switch(elem.m_Type)
		{
		case ShaderDataType::Float:
		case ShaderDataType::Float2:
		case ShaderDataType::Float3:
		case ShaderDataType::Float4:
		{
			glEnableVertexAttribArray(m_VertexBufferIndex);
			glVertexAttribPointer(m_VertexBufferIndex, elem.GetComponentCount(), ShaderDataTypeToOpenGLBaseType(elem.m_Type), elem.m_Normalized ? GL_TRUE : GL_FALSE, static_cast<GLsizei>(layout.GetStride()), (const void*)elem.m_Offset);
			m_VertexBufferIndex++;
			break;
		}
		case ShaderDataType::Int:
		case ShaderDataType::Int2:
		case ShaderDataType::Int3:
		case ShaderDataType::Int4:
		case ShaderDataType::Bool:
		{
			glEnableVertexAttribArray(m_VertexBufferIndex);
			glVertexAttribIPointer(m_VertexBufferIndex, elem.GetComponentCount(), ShaderDataTypeToOpenGLBaseType(elem.m_Type), static_cast<GLsizei>(layout.GetStride()), (const void*)elem.m_Offset);
			m_VertexBufferIndex++;
			break;
		}
		case ShaderDataType::Mat3:
		case ShaderDataType::Mat4:
		{
			uint8_t count = elem.GetComponentCount();
			for (uint8_t i = 0; i < count; i++)
			{
				glEnableVertexAttribArray(m_VertexBufferIndex);
				glVertexAttribPointer(m_VertexBufferIndex, count, ShaderDataTypeToOpenGLBaseType(elem.m_Type), elem.m_Normalized ? GL_TRUE : GL_FALSE, static_cast<GLsizei>(layout.GetStride()), (const void*)(elem.m_Offset + sizeof(float) * count * i));
				glVertexAttribDivisor(m_VertexBufferIndex, 1);
				m_VertexBufferIndex++;
			}
			break;
		}
		default:
			WHP_CORE_ASSERT(false, "Unknown ShaderDataType!");
		}
	}

	m_VertexBuffers.push_back(vertexBuffer);
}

void OpenGLVertexArray::SetIndexBuffer(const Ref<IndexBuffer>& indexBuffer)
{
	WHP_PROFILE_FUNCTION();

	glBindVertexArray(m_RendererID);
	indexBuffer->Bind();
	m_IndexBuffer = indexBuffer;
}


_WHIP_END
