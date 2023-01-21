#include <whippch.h>

#include <glad/glad.h>

#include "OpenGLVertexArray.h"

_WHIP_START

static GLenum ShaderDataTypeToOpenGLBaseType(ShaderDataType type)
{
	switch (type)
	{
		case Whip::ShaderDataType::None:		WHP_CORE_ASSERT(false, "ShaderDataType is None!"); return 0;
		case Whip::ShaderDataType::Float:		return GL_FLOAT;
		case Whip::ShaderDataType::Float2:		return GL_FLOAT;
		case Whip::ShaderDataType::Float3:		return GL_FLOAT;
		case Whip::ShaderDataType::Float4:		return GL_FLOAT;
		case Whip::ShaderDataType::Mat3:		return GL_FLOAT;
		case Whip::ShaderDataType::Mat4:		return GL_FLOAT;
		case Whip::ShaderDataType::Bool:		return GL_BOOL;
		case Whip::ShaderDataType::Int:			return GL_INT;
		case Whip::ShaderDataType::Int2:		return GL_INT;
		case Whip::ShaderDataType::Int3:		return GL_INT;
		case Whip::ShaderDataType::Int4:		return GL_INT;
	}
	WHP_CORE_ASSERT(false, "Unknown ShaderDataType!");
	return 0;
}

OpenGLVertexArray::OpenGLVertexArray()
{
	glCreateVertexArrays(1, &m_RendererID);
}

OpenGLVertexArray::~OpenGLVertexArray()
{
	glDeleteVertexArrays(1, &m_RendererID);
}

void OpenGLVertexArray::Bind() const
{
	glBindVertexArray(m_RendererID);
}

void OpenGLVertexArray::Unbind() const
{
	glBindVertexArray(0);
}

void OpenGLVertexArray::AddVertexBuffer(const std::shared_ptr<VertexBuffer>& vertexBuffer)
{
	WHP_CORE_ASSERT(vertexBuffer->GetLayout().getElements().size(), "Vertex Buffer has no layout");
	
	glBindVertexArray(m_RendererID);
	vertexBuffer->Bind();


	uint32_t index = 0;
	for (const auto& elem : vertexBuffer->GetLayout())
	{
		glEnableVertexAttribArray(index);
		glVertexAttribPointer(index, elem.GetComponentCount(), ShaderDataTypeToOpenGLBaseType(elem.type), elem.normalized ? GL_TRUE : GL_FALSE, vertexBuffer->GetLayout().GetStride(), (const void*)elem.offset);
		index++;
	}

	m_VertexBuffers.push_back(vertexBuffer);
}

void OpenGLVertexArray::SetIndexBuffer(const std::shared_ptr<IndexBuffer>& indexBuffer)
{
	glBindVertexArray(m_RendererID);
	indexBuffer->Bind();
	m_IndexBuffer = indexBuffer;
}


_WHIP_END