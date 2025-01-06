#include <whippch.h>
#include "OpenGLShader.h"

#include <fstream>

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

#include <shaderc/shaderc.hpp>
#include <spirv_cross/spirv_cross.hpp>
#include <spirv_cross/spirv_glsl.hpp>

#include <Whip/Core/utility.h>

#include <coco.h>

_WHIP_START

namespace utils
{

	static GLenum shader_type_from_string(const std::string& type)
	{
		if (type == "vertex")							return GL_VERTEX_SHADER;
		if (type == "fragment" || type == "pixel")		return GL_FRAGMENT_SHADER;

		WHP_CORE_ASSERT(false, "Unknown shader type");
		return 0;
	}

	static shaderc_shader_kind gl_shader_stage_to_shaderc(GLenum stage)
	{
		switch (stage)
		{
		case GL_VERTEX_SHADER:   return shaderc_glsl_vertex_shader;
		case GL_FRAGMENT_SHADER: return shaderc_glsl_fragment_shader;
		}
		WHP_CORE_ASSERT(false, "Unknown shader type");
		return (shaderc_shader_kind)0;
	}

	static const char* gl_shader_stage_to_string(GLenum stage)
	{
		switch (stage)
		{
		case GL_VERTEX_SHADER:   return "GL_VERTEX_SHADER";
		case GL_FRAGMENT_SHADER: return "GL_FRAGMENT_SHADER";
		}
		WHP_CORE_ASSERT(false, "Unknown shader type");
		return nullptr;
	}

	static const char* get_cache_directory()
	{
		// TODO: make sure the assets directory is valid
		return "assets/cache/shader/opengl";
	}

	static void create_cache_directory_if_needed()
	{
		std::string cache_directory = get_cache_directory();
		if (!std::filesystem::exists(cache_directory))
			std::filesystem::create_directories(cache_directory);
	}

	static const char* gl_shader_stage_cached_opengl_file_extension(uint32_t stage)
	{
		switch (stage)
		{
		case GL_VERTEX_SHADER:    return ".cached_opengl.vert";
		case GL_FRAGMENT_SHADER:  return ".cached_opengl.frag";
		}
		WHP_CORE_ASSERT(false, "Unknown shader type");
		return "";
	}

	static const char* gl_shader_stage_cached_vulkan_file_extension(uint32_t stage)
	{
		switch (stage)
		{
		case GL_VERTEX_SHADER:    return ".cached_vulkan.vert";
		case GL_FRAGMENT_SHADER:  return ".cached_vulkan.frag";
		}
		WHP_CORE_ASSERT(false, "Unknown shader type");
		return "";
	}
}

opengl_shader::opengl_shader(const std::string& filepath)
	:m_rendererID(0), m_filepath(filepath)
{
	WHP_PROFILE_FUNCTION();

	std::string source = utils::read_file(filepath);

	auto shader_sources = pre_process(source);

	{
		coco::timer t;
		compile_or_get_vulkan_binaries(shader_sources);
		compile_or_get_opengl_binaries();
		create_program();
		t.stop();
		WHP_CORE_WARN("[OpenGL Shader] Shader creation took {0} ms", t.get_casted_time<coco::time_units::milliseconds>());
	}

	m_name = utils::fetch_filename(filepath);
}

opengl_shader::opengl_shader(const std::string& name, const std::string& filepath)
	: m_rendererID(0), m_name(name), m_filepath(filepath)
{
	WHP_PROFILE_FUNCTION();

	std::string source = utils::read_file(filepath);
	auto shader_sources = pre_process(source);

	{
		coco::timer t;
		compile_or_get_vulkan_binaries(shader_sources);
		compile_or_get_opengl_binaries();
		create_program();
		t.stop();
		WHP_CORE_WARN("[OpenGL Shader] Shader creation took {0} ms", t.get_casted_time<coco::time_units::milliseconds>());
	}
}

opengl_shader::opengl_shader(const std::string& name, const std::string& vertex_filepath, const std::string& fragment_filepath)
	:m_rendererID(0), m_name(name)
{
	WHP_PROFILE_FUNCTION();

	std::string vertex_source = utils::read_file(vertex_filepath);
	std::string fragment_source = utils::read_file(fragment_filepath);
	std::unordered_map<GLenum, std::string> sources;
	sources[GL_VERTEX_SHADER] = vertex_source;
	sources[GL_FRAGMENT_SHADER] = fragment_source;

	{
		coco::timer t;
		compile_or_get_vulkan_binaries(sources);
		compile_or_get_opengl_binaries();
		create_program();
		t.stop();
		WHP_CORE_WARN("[OpenGL Shader] Shader creation took {0} ms", t.get_casted_time<coco::time_units::milliseconds>());
	}
}

opengl_shader::~opengl_shader()
{
	glDeleteProgram(m_rendererID);
}

std::unordered_map<GLenum, std::string> opengl_shader::pre_process(const std::string& source)
{
	WHP_PROFILE_FUNCTION();

	std::unordered_map<GLenum, std::string> shader_sources;
	const char* type_token = "#type";
	size_t type_token_length = strlen(type_token);
	size_t pos = source.find(type_token, 0);
	while (pos != std::string::npos)
	{
		size_t eol = source.find_first_of("\r\n", pos);
		WHP_CORE_ASSERT(eol != std::string::npos, "[OpenGL Shader] Syntax Error!");
		size_t begin = pos + type_token_length + 1;
		std::string type = source.substr(begin, eol - begin);
		type.erase(std::remove(type.begin(), type.end(), ' '), type.end());
		WHP_CORE_ASSERT(utils::shader_type_from_string(type), "[OpenGL Shader] Invalid shader type specifier!");
		size_t next_line_pos = source.find_first_not_of("\r\n", eol);
		pos = source.find(type_token, next_line_pos);
		shader_sources[utils::shader_type_from_string(type)] = source.substr(next_line_pos, pos - (next_line_pos) == std::string::npos ? source.size() - 1 : pos - next_line_pos);
	}
	return shader_sources;
}

void opengl_shader::compile_or_get_vulkan_binaries(const std::unordered_map<GLenum, std::string>& shader_sources)
{
	renderer_id_t program = glCreateProgram();

	shaderc::Compiler compiler;
	shaderc::CompileOptions options;
	options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);
	const bool optimize = true;
	if (optimize)
		options.SetOptimizationLevel(shaderc_optimization_level_performance);
	std::filesystem::path cacheDirectory = utils::get_cache_directory();
	auto& shader_data = m_vulkanSPIRV;
	shader_data.clear();
	for (auto&& [stage, source] : shader_sources)
	{
		std::filesystem::path shaderFilePath = m_filepath;
		std::filesystem::path cachedPath = cacheDirectory / (shaderFilePath.filename().string() + utils::gl_shader_stage_cached_vulkan_file_extension(stage));

		std::ifstream in(cachedPath, std::ios::in | std::ios::binary);
		if (in.is_open())
		{
			in.seekg(0, std::ios::end);
			auto size = in.tellg();
			in.seekg(0, std::ios::beg);
			auto& data = shader_data[stage];
			data.resize(size / sizeof(uint32_t));
			in.read((char*)data.data(), size);
		}
		else
		{
			shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(source, utils::gl_shader_stage_to_shaderc(stage), m_filepath.c_str(), options);
			if (module.GetCompilationStatus() != shaderc_compilation_status_success)
			{
				WHP_CORE_ERROR(module.GetErrorMessage());
				WHP_CORE_ASSERT(false, "");
			}

			shader_data[stage] = std::vector<uint32_t>(module.cbegin(), module.cend());

			std::ofstream out(cachedPath, std::ios::out | std::ios::binary);
			if (out.is_open())
			{
				auto& data = shader_data[stage];
				out.write((char*)data.data(), data.size() * sizeof(uint32_t));
				out.flush();
				out.close();
			}
		}
		for (auto&& [sd_stage, sd_data] : shader_data)
			reflect(sd_stage, sd_data);
	}
}

void opengl_shader::compile_or_get_opengl_binaries()
{
	auto& shaderData = m_openglSPIRV;

	shaderc::Compiler compiler;
	shaderc::CompileOptions options;
	options.SetTargetEnvironment(shaderc_target_env_opengl, shaderc_env_version_opengl_4_5);
	const bool optimize = false;
	if (optimize)
		options.SetOptimizationLevel(shaderc_optimization_level_performance);

	std::filesystem::path cacheDirectory = utils::get_cache_directory();

	shaderData.clear();
	m_opengl_source_code.clear();
	for (auto&& [stage, spirv] : m_vulkanSPIRV)
	{
		std::filesystem::path shaderFilePath = m_filepath;
		std::filesystem::path cachedPath = cacheDirectory / (shaderFilePath.filename().string() + utils::gl_shader_stage_cached_opengl_file_extension(stage));

		std::ifstream in(cachedPath, std::ios::in | std::ios::binary);
		if (in.is_open())
		{
			in.seekg(0, std::ios::end);
			auto size = in.tellg();
			in.seekg(0, std::ios::beg);

			auto& data = shaderData[stage];
			data.resize(size / sizeof(uint32_t));
			in.read((char*)data.data(), size);
		}
		else
		{
			spirv_cross::CompilerGLSL glslCompiler(spirv);
			m_opengl_source_code[stage] = glslCompiler.compile();
			auto& source = m_opengl_source_code[stage];

			shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(source, utils::gl_shader_stage_to_shaderc(stage), m_filepath.c_str());
			if (module.GetCompilationStatus() != shaderc_compilation_status_success)
			{
				WHP_CORE_ERROR(module.GetErrorMessage());
				WHP_CORE_ASSERT(false, "");
			}

			shaderData[stage] = std::vector<uint32_t>(module.cbegin(), module.cend());

			std::ofstream out(cachedPath, std::ios::out | std::ios::binary);
			if (out.is_open())
			{
				auto& data = shaderData[stage];
				out.write((char*)data.data(), data.size() * sizeof(uint32_t));
				out.flush();
				out.close();
			}
		}
	}
}

void opengl_shader::create_program()
{
	GLuint program = glCreateProgram();

	std::vector<GLuint> shaderIDs;
	for (auto&& [stage, spirv] : m_openglSPIRV)
	{
		GLuint shaderID = shaderIDs.emplace_back(glCreateShader(stage));
		glShaderBinary(1, &shaderID, GL_SHADER_BINARY_FORMAT_SPIR_V, spirv.data(), (GLsizei)spirv.size() * sizeof(uint32_t));
		glSpecializeShader(shaderID, "main", 0, nullptr, nullptr);
		glAttachShader(program, shaderID);
	}

	glLinkProgram(program);

	GLint isLinked;
	glGetProgramiv(program, GL_LINK_STATUS, &isLinked);
	if (isLinked == GL_FALSE)
	{
		GLint maxLength;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);

		std::vector<GLchar> infoLog(maxLength);
		glGetProgramInfoLog(program, maxLength, &maxLength, infoLog.data());
		WHP_CORE_ERROR("[OpenGL Shader] Shader linking failed ({0}):\n{1}", m_filepath, infoLog.data());

		glDeleteProgram(program);

		for (auto id : shaderIDs)
			glDeleteShader(id);
	}

	for (auto id : shaderIDs)
	{
		glDetachShader(program, id);
		glDeleteShader(id);
	}

	m_rendererID = program;
}

void opengl_shader::reflect(GLenum stage, const std::vector<uint32_t>& shaderData)
{
	spirv_cross::Compiler compiler(shaderData);
	spirv_cross::ShaderResources resources = compiler.get_shader_resources();

	WHP_CORE_TRACE("[OpenGL Shader] Reflect - {0} {1}", utils::gl_shader_stage_to_string(stage), m_filepath);

	for (const auto& resource : resources.uniform_buffers)
	{
		const auto& bufferType = compiler.get_type(resource.base_type_id);
		uint32_t bufferSize = (uint32_t)compiler.get_declared_struct_size(bufferType);
		uint32_t binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
		int memberCount = (int)bufferType.member_types.size();
	}
}

void opengl_shader::bind() const
{
	WHP_PROFILE_FUNCTION();

	glUseProgram(m_rendererID);
}

void opengl_shader::unbind() const
{
	WHP_PROFILE_FUNCTION();

	glUseProgram(0);
}

void opengl_shader::set_int(const std::string& name, int value)
{
	WHP_PROFILE_FUNCTION();

	upload_uniform_int(name, value);
}

void opengl_shader::set_int_array(const std::string& name, int* values, uint32_t count)
{
	WHP_PROFILE_FUNCTION();

	upload_uniform_int_array(name, values, count);
}

void opengl_shader::set_float(const std::string& name, float value)
{
	WHP_PROFILE_FUNCTION();

	upload_uniform_float(name, value);
}

void opengl_shader::set_float2(const std::string& name, const glm::vec2& value)
{
	WHP_PROFILE_FUNCTION();

	upload_uniform_float2(name, value);
}

void opengl_shader::set_float3(const std::string& name, const glm::vec3& value)
{
	WHP_PROFILE_FUNCTION();

	upload_uniform_float3(name, value);
}

void opengl_shader::set_float4(const std::string& name, const glm::vec4& value)
{
	WHP_PROFILE_FUNCTION();

	upload_uniform_float4(name, value);
}

void opengl_shader::set_mat4(const std::string& name, const glm::mat4& value)
{
	WHP_PROFILE_FUNCTION();

	upload_uniform_mat4(name, value);
}

void opengl_shader::set_double(const std::string& name, double value)
{
	WHP_PROFILE_FUNCTION();

	upload_uniform_double(name, value);
}

void opengl_shader::upload_uniform_mat3(const std::string& name, const glm::mat3& matrix) const
{
	int location = glGetUniformLocation(m_rendererID, name.c_str());
	glUniformMatrix3fv(location, 1, GL_FALSE, glm::value_ptr(matrix));
}

void opengl_shader::upload_uniform_mat4(const std::string& name, const glm::mat4& matrix) const
{
	GLint location = glGetUniformLocation(m_rendererID, name.c_str());
	glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(matrix));
}

void opengl_shader::upload_uniform_int(const std::string& name, int value) const
{
	int location = glGetUniformLocation(m_rendererID, name.c_str());
	glUniform1i(location, value);
}

void opengl_shader::upload_uniform_int_array(const std::string& name, int* values, uint32_t count) const
{
	int location = glGetUniformLocation(m_rendererID, name.c_str());
	glUniform1iv(location, count, values);
}

void opengl_shader::upload_uniform_float(const std::string& name, float value) const
{
	int location = glGetUniformLocation(m_rendererID, name.c_str());
	glUniform1f(location, value);
}

void opengl_shader::upload_uniform_float2(const std::string& name, const glm::vec2& vec) const
{
	int location = glGetUniformLocation(m_rendererID, name.c_str());
	glUniform2f(location, vec.x, vec.y);
}

void opengl_shader::upload_uniform_float3(const std::string& name, const glm::vec3& vec) const
{
	int location = glGetUniformLocation(m_rendererID, name.c_str());
	glUniform3f(location, vec.r, vec.g, vec.b);
}

void opengl_shader::upload_uniform_float4(const std::string& name, const glm::vec4& vec) const
{
	int location = glGetUniformLocation(m_rendererID, name.c_str());
	glUniform4f(location, vec.r, vec.g, vec.b, vec.a);
}

void opengl_shader::upload_uniform_double(const std::string& name, double value) const
{
	int location = glGetUniformLocation(m_rendererID, name.c_str());
	glUniform1d(location, value);
}


_WHIP_END
