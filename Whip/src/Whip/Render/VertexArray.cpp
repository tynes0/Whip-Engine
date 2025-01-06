#include <whippch.h>
#include "VertexArray.h"

#include <Platform/OpenGL/OpenGLVertexArray.h>
#include "Renderer.h"

_WHIP_START

ref<vertex_array> vertex_array::create()
{
	switch (renderer::get_API())
	{
	case render_API::API::none:		WHP_CORE_ASSERT(false, "RandererAPI is none!"); return nullptr;
	case render_API::API::opengl:	return make_ref<opengl_vertex_array>();
	}

	WHP_CORE_ASSERT(false, "Unknown RendererAPI");
	return nullptr;
}

_WHIP_END