#include "whippch.h"
#include "Shader.h"

#include "Renderer.h"
#include <Platform/OpenGL/OpenGLShader.h>

_WHIP_START

ref<Shader> Shader::create(const std::string& filepath)
{
	switch (Renderer::GetAPI())
	{
	case RenderAPI::API::None:		WHP_CORE_ASSERT(false, "RandererAPI is none!"); return nullptr;
	case RenderAPI::API::OpenGL:	return make_ref<OpenGLShader>(filepath);
	}

	WHP_CORE_ASSERT(false, "Unknown RendererAPI");
	return nullptr;
}

ref<Shader> Shader::create(const std::string& vertex_filepath, const std::string& fragment_filepath)
{
	switch (Renderer::GetAPI())
	{
	case RenderAPI::API::None:		WHP_CORE_ASSERT(false, "RandererAPI is none!"); return nullptr;
	case RenderAPI::API::OpenGL:	return make_ref<OpenGLShader>(vertex_filepath, fragment_filepath);
	}

	WHP_CORE_ASSERT(false, "Unknown RendererAPI");
	return nullptr;
}

ref<Shader> Shader::create(const std::string& vertexSrc, const std::string& fragmentSrc, short test)
{
	switch (Renderer::GetAPI())
	{
		case RenderAPI::API::None:		WHP_CORE_ASSERT(false, "RandererAPI is none!"); return nullptr;
		case RenderAPI::API::OpenGL:	return make_ref<OpenGLShader>(vertexSrc, fragmentSrc, test);
	}

	WHP_CORE_ASSERT(false, "Unknown RendererAPI");
	return nullptr;
}

_WHIP_END