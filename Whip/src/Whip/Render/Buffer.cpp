#include "whippch.h"
#include "Buffer.h"

#include "Renderer.h"
#include <Platform/OpenGL/OpenGLBuffer.h>

_WHIP_START


ref<vertex_buffer> vertex_buffer::create(float* vertices, uint32_t size)
{
	switch (renderer::get_API())
	{
	case render_API::API::none:			WHP_CORE_ASSERT(false, "RendererAPI is none!"); return nullptr;
	case render_API::API::opengl:		return make_ref<opengl_vertex_buffer>(vertices, size);
	}

	WHP_CORE_ASSERT(false, "Unknown RendererAPI!");
	return nullptr;
}

WHP_NODISCARD ref<vertex_buffer> vertex_buffer::create(uint32_t size)
{
	switch (renderer::get_API())
	{
	case render_API::API::none:			WHP_CORE_ASSERT(false, "RendererAPI is none!"); return nullptr;
	case render_API::API::opengl:		return make_ref<opengl_vertex_buffer>(size);
	}

	WHP_CORE_ASSERT(false, "Unknown RendererAPI!");
	return nullptr;
}

ref<index_buffer> index_buffer::create(uint32_t* indices, uint32_t size)
{
	switch (renderer::get_API())
	{
	case render_API::API::none:			WHP_CORE_ASSERT(false, "RendererAPI is none!"); return nullptr;
	case render_API::API::opengl:		return make_ref<opengl_index_buffer>(indices, size);
	}

	WHP_CORE_ASSERT(false, "Unknown RendererAPI!");
	return nullptr;
}

_WHIP_END