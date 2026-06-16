#include <WhipPch.h>
#include <Whip/Render/VertexArray.h>

#include <Platform/OpenGL/OpenGLVertexArray.h>
#include <Whip/Render/Renderer.h>

_WHIP_START

Ref<VertexArray> VertexArray::Create()
{
	switch (Renderer::GetAPI())
	{
	case RenderAPI::API::None:		WHP_CORE_ASSERT(false, "RandererAPI is none!"); return nullptr;
	case RenderAPI::API::OpenGL:	return MakeRef<OpenGLVertexArray>();
	}

	WHP_CORE_ASSERT(false, "Unknown RendererAPI");
	return nullptr;
}

_WHIP_END