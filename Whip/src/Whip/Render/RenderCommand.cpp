#include "WhipPch.h"
#include <Whip/Render/RenderCommand.h>

#include <Platform/OpenGL/OpenGLRenderAPI.h>

_WHIP_START

RenderAPI* RenderCommand::s_RenderAPI = MakeRawTagged<OpenGLRenderAPI>(memory::MemoryTag::Renderer);

_WHIP_END
