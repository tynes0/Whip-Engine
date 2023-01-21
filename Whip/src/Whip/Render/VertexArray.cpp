#include <whippch.h>
#include "VertexArray.h"

#include <Platform/OpenGL/OpenGLVertexArray.h>
#include "Renderer.h"

_WHIP_START

VertexArray* VertexArray::Create()
{
	switch (Renderer::GetRendererAPI())
	{
	case RendererAPI::None:		WHP_CORE_ASSERT(false, "RandererAPI is none!"); return nullptr;
	case RendererAPI::OpenGL:	return new OpenGLVertexArray();
	}

	WHP_CORE_ASSERT(false, "Unknown RendererAPI");
	return nullptr;
}

_WHIP_END