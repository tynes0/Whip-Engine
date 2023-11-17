#include "whippch.h"
#include "Texture.h"

#include <Whip/Render/Renderer.h>
#include <Platform/OpenGL/OpenGLTexture.h>

_WHIP_START

ref<texture2D> texture2D::create(uint32_t width, uint32_t height)
{
	switch (renderer::get_API())
	{
	case render_API::API::none:		WHP_CORE_ASSERT(false, "RandererAPI is none!"); return nullptr;
	case render_API::API::opengl:	return make_ref<opengl_texture2D>(width, height);
	}

	WHP_CORE_ASSERT(false, "Unknown RendererAPI");
	return nullptr;
}


ref<texture2D> texture2D::create(const std::string& path)
{
	switch (renderer::get_API())
	{
	case render_API::API::none:		WHP_CORE_ASSERT(false, "RandererAPI is none!"); return nullptr;
	case render_API::API::opengl:	return make_ref<opengl_texture2D>(path);
	}

	WHP_CORE_ASSERT(false, "Unknown RendererAPI");
	return nullptr;
}


_WHIP_END