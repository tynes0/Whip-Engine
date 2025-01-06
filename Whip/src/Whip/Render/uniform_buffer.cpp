#include "whippch.h"
#include "uniform_buffer.h"

#include "Renderer.h"
#include <Platform/OpenGL/opengl_uniform_buffer.h>

_WHIP_START

ref<uniform_buffer> uniform_buffer::create(uint32_t size, uint32_t binding)
{
	switch (renderer::get_API())
	{
	case render_API::API::none: WHP_CORE_ASSERT(false, "Render_API::none is currently not supported!"); return nullptr;
	case render_API::API::opengl: return make_ref<opengl_uniform_buffer>(size, binding);
	}
	WHP_CORE_ASSERT(false, "Unknown render_API!");
	return nullptr;
}

_WHIP_END
