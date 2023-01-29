#include "whippch.h"
#include "Texture.h"

#include <Whip/Render/Renderer.h>
#include <Platform/OpenGL/OpenGLTexture.h>

_WHIP_START

ref<Texture2D> Texture2D::create(const std::string& path)
{
	switch (Renderer::GetAPI())
	{
	case RenderAPI::API::None:		WHP_CORE_ASSERT(false, "RandererAPI is none!"); return nullptr;
	case RenderAPI::API::OpenGL:	return make_ref<OpenGLTexture2D>(path);
	}

	WHP_CORE_ASSERT(false, "Unknown RendererAPI");
	return nullptr;
}


_WHIP_END