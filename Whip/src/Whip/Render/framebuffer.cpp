#include "WhipPch.h"
#include <Whip/Render/Framebuffer.h>
#include <Whip/Render/Renderer.h>

#include "Platform/OpenGL/OpenGLFramebuffer.h"
#include "Platform/OpenGL/OpenGLBuffer.h"

_WHIP_START

Ref<Framebuffer> Framebuffer::Create(const FramebufferSpecification& spec)
{
	switch (Renderer::GetAPI())
	{
	case RenderAPI::API::None:			WHP_CORE_ASSERT(false, "RendererAPI is none!"); return nullptr;
	case RenderAPI::API::OpenGL:		return MakeRef<OpenGLFramebuffer>(spec);
	}

	WHP_CORE_ASSERT(false, "Unknown RendererAPI!");
	return nullptr;
}

_WHIP_END
