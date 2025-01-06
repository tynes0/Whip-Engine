#include "whippch.h"
#include "Shader.h"

#include "Renderer.h"
#include <Platform/OpenGL/OpenGLShader.h>

_WHIP_START

ref<shader> shader::create(const std::string& filepath)
{
	switch (renderer::get_API())
	{
	case render_API::API::none:		WHP_CORE_ASSERT(false, "RandererAPI is none!"); return nullptr;
	case render_API::API::opengl:	return make_ref<opengl_shader>(filepath);
	}

	WHP_CORE_ASSERT(false, "Unknown RendererAPI");
	return nullptr;
}

ref<shader> shader::create(const std::string& name, const std::string& filepath)
{
	switch (renderer::get_API())
	{
	case render_API::API::none:		WHP_CORE_ASSERT(false, "RandererAPI is none!"); return nullptr;
	case render_API::API::opengl:	return make_ref<opengl_shader>(name, filepath);
	}

	WHP_CORE_ASSERT(false, "Unknown RendererAPI");
	return nullptr;
}

ref<shader> shader::create(const std::string& name, const std::string& vertex_filepath, const std::string& fragment_filepath)
{
	switch (renderer::get_API())
	{
	case render_API::API::none:		WHP_CORE_ASSERT(false, "RandererAPI is none!"); return nullptr;
	case render_API::API::opengl:	return make_ref<opengl_shader>(name, vertex_filepath, fragment_filepath);
	}

	WHP_CORE_ASSERT(false, "Unknown RendererAPI");
	return nullptr;
}

void shader_library::add(const std::string& name, const ref<shader>& shader)
{
	WHP_CORE_ASSERT(!exist(name), "Shader already exist!");
	m_shaders[name] = shader;
}

void shader_library::add(const ref<shader>& shader)
{
	auto& name = shader->get_name();
	add(name, shader);
}

ref<shader> shader_library::load(const std::string& filepath)
{
	auto shad = shader::create(filepath);
	add(shad);
	return shad;
}

ref<shader> shader_library::load(const std::string& name, const std::string& filepath)
{
	auto shad = shader::create(name, filepath);
	add(name, shad);
	return shad;
}

ref<shader> shader_library::load(const std::string& name, const std::string& vertex_filepath, const std::string& fragment_filepath)
{
	auto shad = shader::create(name, vertex_filepath, fragment_filepath);
	add(name, shad);
	return shad;
}

ref<shader> shader_library::get(const std::string& name)
{
	WHP_CORE_ASSERT(exist(name), "Shader is doesn't exist!");
	return m_shaders[name];
}

bool shader_library::exist(const std::string& name) const
{
	return m_shaders.find(name) != m_shaders.end();
}

_WHIP_END
