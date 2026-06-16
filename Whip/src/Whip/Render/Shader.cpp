#include "WhipPch.h"
#include <Whip/Render/Shader.h>

#include <Whip/Render/Renderer.h>
#include <Platform/OpenGL/OpenGLShader.h>

_WHIP_START

Ref<Shader> Shader::Create(const std::string& filepath)
{
	switch (Renderer::GetAPI())
	{
	case RenderAPI::API::None:		WHP_CORE_ASSERT(false, "RandererAPI is none!"); return nullptr;
	case RenderAPI::API::OpenGL:	return MakeRef<OpenGLShader>(filepath);
	}

	WHP_CORE_ASSERT(false, "Unknown RendererAPI");
	return nullptr;
}

Ref<Shader> Shader::Create(const std::string& name, const std::string& filepath)
{
	switch (Renderer::GetAPI())
	{
	case RenderAPI::API::None:		WHP_CORE_ASSERT(false, "RandererAPI is none!"); return nullptr;
	case RenderAPI::API::OpenGL:	return MakeRef<OpenGLShader>(name, filepath);
	}

	WHP_CORE_ASSERT(false, "Unknown RendererAPI");
	return nullptr;
}

Ref<Shader> Shader::Create(const std::string& name, const std::string& vertexFilepath, const std::string& fragmentFilepath)
{
	switch (Renderer::GetAPI())
	{
	case RenderAPI::API::None:		WHP_CORE_ASSERT(false, "RandererAPI is none!"); return nullptr;
	case RenderAPI::API::OpenGL:	return MakeRef<OpenGLShader>(name, vertexFilepath, fragmentFilepath);
	}

	WHP_CORE_ASSERT(false, "Unknown RendererAPI");
	return nullptr;
}

void ShaderLibrary::Add(const std::string& name, const Ref<Shader>& shader)
{
	WHP_CORE_ASSERT(!Exist(name), "Shader already exist!");
	m_Shaders[name] = shader;
}

void ShaderLibrary::Add(const Ref<Shader>& shader)
{
	auto& name = shader->GetName();
	Add(name, shader);
}

Ref<Shader> ShaderLibrary::Load(const std::string& filepath)
{
	auto shad = Shader::Create(filepath);
	Add(shad);
	return shad;
}

Ref<Shader> ShaderLibrary::Load(const std::string& name, const std::string& filepath)
{
	auto shad = Shader::Create(name, filepath);
	Add(name, shad);
	return shad;
}

Ref<Shader> ShaderLibrary::Load(const std::string& name, const std::string& vertexFilepath, const std::string& fragmentFilepath)
{
	auto shad = Shader::Create(name, vertexFilepath, fragmentFilepath);
	Add(name, shad);
	return shad;
}

Ref<Shader> ShaderLibrary::Get(const std::string& name)
{
	WHP_CORE_ASSERT(Exist(name), "Shader is doesn't exist!");
	return m_Shaders[name];
}

bool ShaderLibrary::Exist(const std::string& name) const
{
	return m_Shaders.find(name) != m_Shaders.end();
}

_WHIP_END
