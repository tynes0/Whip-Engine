#include "WhipPch.h"
#include <Whip/Render/Buffer.h>

#include <Whip/Render/Renderer.h>
#include <Platform/OpenGL/OpenGLBuffer.h>

_WHIP_START


Ref<VertexBuffer> VertexBuffer::Create(float* vertices, uint32_t size)
{
	switch (Renderer::GetAPI())
	{
	case RenderAPI::API::None:			WHP_CORE_ASSERT(false, "RendererAPI is none!"); return nullptr;
	case RenderAPI::API::OpenGL:		return MakeRef<OpenGLVertexBuffer>(vertices, size);
	}

	WHP_CORE_ASSERT(false, "Unknown RendererAPI!");
	return nullptr;
}

WHP_NODISCARD Ref<VertexBuffer> VertexBuffer::Create(uint32_t size)
{
	switch (Renderer::GetAPI())
	{
	case RenderAPI::API::None:			WHP_CORE_ASSERT(false, "RendererAPI is none!"); return nullptr;
	case RenderAPI::API::OpenGL:		return MakeRef<OpenGLVertexBuffer>(size);
	}

	WHP_CORE_ASSERT(false, "Unknown RendererAPI!");
	return nullptr;
}

Ref<IndexBuffer> IndexBuffer::Create(uint32_t* indices, uint32_t size)
{
	switch (Renderer::GetAPI())
	{
	case RenderAPI::API::None:			WHP_CORE_ASSERT(false, "RendererAPI is none!"); return nullptr;
	case RenderAPI::API::OpenGL:		return MakeRef<OpenGLIndexBuffer>(indices, size);
	}

	WHP_CORE_ASSERT(false, "Unknown RendererAPI!");
	return nullptr;
}

_WHIP_END