#include "whippch.h"
#include "RenderCommand.h"

#include <Platform/OpenGL/OpenGLRenderAPI.h>

_WHIP_START

render_API* render_command::s_render_API = new opengl_render_API();

_WHIP_END