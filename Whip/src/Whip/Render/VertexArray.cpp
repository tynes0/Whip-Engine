#include <whippch.h>
#include "VertexArray.h"

#include <Platform/OpenGL/OpenGLVertexArray.h>
#include "Renderer.h"

_WHIP_START

VertexArray* VertexArray::Create()
{
	switch (Renderer::GetAPI())
	{
	case RenderAPI::API::None:		WHP_CORE_ASSERT(false, "RandererAPI is none!"); return nullptr;
	case RenderAPI::API::OpenGL:	return new OpenGLVertexArray();
	}

	WHP_CORE_ASSERT(false, "Unknown RendererAPI");
	return nullptr;
}

_WHIP_END