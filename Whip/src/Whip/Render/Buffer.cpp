#include "whippch.h"
#include "Buffer.h"

#include "Renderer.h"
#include <Platform/OpenGL/OpenGLBuffer.h>

_WHIP_START


ref<VertexBuffer> VertexBuffer::Create(float* vertices, uint32_t size)
{
	switch (Renderer::GetAPI())
	{
		case RenderAPI::API::None:			WHP_CORE_ASSERT(false, "RendererAPI is none!"); return nullptr;
		case RenderAPI::API::OpenGL:		return make_ref<OpenGLVertexBuffer>(vertices, size);
	}

	WHP_CORE_ASSERT(false, "Unknown RendererAPI!");
	return nullptr;
}

ref<IndexBuffer> IndexBuffer::Create(uint32_t* indices, uint32_t size)
{
	switch (Renderer::GetAPI())
	{
	case RenderAPI::API::None:			WHP_CORE_ASSERT(false, "RendererAPI is none!"); return nullptr;
	case RenderAPI::API::OpenGL:		return make_ref<OpenGLIndexBuffer>(indices, size);
	}

	WHP_CORE_ASSERT(false, "Unknown RendererAPI!");
	return nullptr;
}

_WHIP_END