#include "WhipPch.h"
#include <Whip/Render/UniformBuffer.h>

#include <Whip/Render/Renderer.h>
#include <Platform/OpenGL/OpenGLUniformBuffer.h>

_WHIP_START

Ref<UniformBuffer> UniformBuffer::Create(uint32_t size, uint32_t binding)
{
	switch (Renderer::GetAPI())
	{
	case RenderAPI::API::None: WHP_CORE_ASSERT(false, "Render_API::none is currently not supported!"); return nullptr;
	case RenderAPI::API::OpenGL: return MakeRef<OpenGLUniformBuffer>(size, binding);
	}
	WHP_CORE_ASSERT(false, "Unknown RenderAPI!");
	return nullptr;
}

_WHIP_END
