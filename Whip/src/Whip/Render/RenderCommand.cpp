#include "WhipPch.h"
#include <Whip/Render/RenderCommand.h>

#include <Platform/OpenGL/OpenGLRenderAPI.h>

_WHIP_START

RenderAPI* RenderCommand::s_RenderAPI = new OpenGLRenderAPI();

_WHIP_END
