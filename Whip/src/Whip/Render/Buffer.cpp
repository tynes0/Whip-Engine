#include "whippch.h"
#include "Buffer.h"

#include "Renderer.h"
#include <Platform/OpenGL/OpenGLBuffer.h>

_WHIP_START


VertexBuffer* VertexBuffer::Create(float* vertices, uint32_t size)
{
	switch (Renderer::GetRendererAPI())
	{
		case RendererAPI::None:			WHP_CORE_ASSERT(false, "RendererAPI is none!"); return nullptr;
		case RendererAPI::OpenGL:		return new OpenGLVertexBuffer(vertices, size);
	}

	WHP_CORE_ASSERT(false, "Unknown RendererAPI!");
	return nullptr;
}

IndexBuffer* IndexBuffer::Create(renderer_id_t* indices, uint32_t size)
{
	switch (Renderer::GetRendererAPI())
	{
	case RendererAPI::None:			WHP_CORE_ASSERT(false, "RendererAPI is none!"); return nullptr;
	case RendererAPI::OpenGL:		return new OpenGLIndexBuffer(indices, size);
	}

	WHP_CORE_ASSERT(false, "Unknown RendererAPI!");
	return nullptr;
}

_WHIP_END