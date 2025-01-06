#include "whippch.h"
#include "framebuffer.h"
#include "Renderer.h"

#include "Platform/OpenGL/opengl_framebuffer.h"
#include "Platform/OpenGL/OpenGLBuffer.h"

_WHIP_START

ref<framebuffer> framebuffer::create(const framebuffer_specification& spec)
{
	switch (renderer::get_API())
	{
	case render_API::API::none:			WHP_CORE_ASSERT(false, "RendererAPI is none!"); return nullptr;
	case render_API::API::opengl:		return make_ref<opengl_framebuffer>(spec);
	}

	WHP_CORE_ASSERT(false, "Unknown RendererAPI!");
	return nullptr;
}

_WHIP_END