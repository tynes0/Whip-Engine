#include <whippch.h>
#include "OpenGLShader.h"

#include <fstream>

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

_WHIP_START

static GLenum shader_type_from_string(const std::string& type)
{
	if (type == "vertex")							return GL_VERTEX_SHADER;
	if (type == "fragment" || type == "pixel")		return GL_FRAGMENT_SHADER;

	WHP_CORE_ASSERT(false, "Unknown shader type '{0}'", type);
	return 0;
}

OpenGLShader::OpenGLShader(const std::string& filepath)
	:m_RendererID(0)
{
	std::string source = read_file(filepath);
	auto shader_sources = pre_process(source);
	compile(shader_sources);
}

OpenGLShader::OpenGLShader(const std::string& vertex_filepath, const std::string& fragment_filepath)
	:m_RendererID(0)
{
	std::string vertex_source = read_file(vertex_filepath);
	std::string fragment_source = read_file(fragment_filepath);
	std::unordered_map<GLenum, std::string> sources;
	sources[GL_VERTEX_SHADER] = vertex_source;
	sources[GL_FRAGMENT_SHADER] = fragment_source;
	compile(sources);
}

OpenGLShader::OpenGLShader(const std::string& vertex_source, const std::string& fragment_source, short _Test)
	: m_RendererID(0)
{
	std::unordered_map<GLenum, std::string> sources;
	sources[GL_VERTEX_SHADER] = vertex_source;
	sources[GL_FRAGMENT_SHADER] = fragment_source;
	compile(sources);
}

OpenGLShader::~OpenGLShader()
{
	glDeleteProgram(m_RendererID);
}

std::string OpenGLShader::read_file(const std::string& filepath)
{
	std::string result;
	std::ifstream in(filepath, std::ios::in | std::ios::binary);
	if (in)
	{
		in.seekg(0, std::ios::end);
		result.resize(in.tellg());
		in.seekg(0, std::ios::beg);
		in.read(&result[0], result.size());
		in.close();
	}
	else
	{
		WHP_CORE_ERROR("Could not open file '{0}'", filepath);
	}
	return result;
}

std::unordered_map<GLenum, std::string> OpenGLShader::pre_process(const std::string& source)
{
	std::unordered_map<GLenum, std::string> shader_sources;
	const char* type_token = "#type";
	size_t type_token_length = strlen(type_token);
	size_t pos = source.find(type_token, 0);
	while (pos != std::string::npos)
	{
		size_t eol = source.find_first_of("\r\n", pos);
		WHP_CORE_ASSERT(eol != std::string::npos, "Syntax Error!");
		size_t begin = pos + type_token_length + 1;
		std::string type = source.substr(begin, eol - begin);
		WHP_CORE_ASSERT(shader_type_from_string(type), "Invalid shader type specifier!");
		size_t next_line_pos = source.find_first_not_of("\r\n", eol);
		pos = source.find(type_token, next_line_pos);
		shader_sources[shader_type_from_string(type)] = source.substr(next_line_pos, pos - (next_line_pos) == std::string::npos ? source.size() - 1 : pos - next_line_pos);
	}
	return shader_sources;
}

void OpenGLShader::compile(const std::unordered_map<GLenum, std::string>& shader_sources)
{
	renderer_id_t program = glCreateProgram();
	std::vector<GLenum> gl_shader_IDs(shader_sources.size());
	for (auto& [type, source] : shader_sources)
	{
		GLuint shader = glCreateShader(type);
		const char* source_c_str = source.c_str();
		glShaderSource(shader, 1, &source_c_str, 0);

		glCompileShader(shader);

		int is_compiled = 0;

		glGetShaderiv(shader, GL_COMPILE_STATUS, &is_compiled);
		if (is_compiled == GL_FALSE)
		{
			int max_length = 0;
			glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &max_length);

			std::vector<char> info_log(max_length);
			glGetShaderInfoLog(shader, max_length, &max_length, &info_log[0]);

			glDeleteShader(shader);

			WHP_CORE_ERROR("{0}", info_log.data());
			WHP_CORE_ASSERT(false, "shader compilation failure!");
			break;
		}
		glAttachShader(program, shader);
		gl_shader_IDs.push_back(shader);
	}

	glLinkProgram(program);

	// Note the different functions here: glGetProgram* instead of glGetShader*
	int is_linked = 0;
	glGetProgramiv(program, GL_LINK_STATUS, &is_linked);
	if (is_linked == GL_FALSE)
	{
		int max_length = 0;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &max_length);

		std::vector<char> info_log(max_length);
		glGetProgramInfoLog(program, max_length, &max_length, &info_log[0]);

		glDeleteProgram(program);

		for (auto id : gl_shader_IDs)
		{
			glDeleteShader(id);
		}

		WHP_CORE_ERROR("{0}", info_log.data());
		WHP_CORE_ASSERT(false, "Shader link failure!");
		return;
	}
	for (auto id : gl_shader_IDs)
	{
		glDetachShader(program, id);
	}
	
	m_RendererID = program;
}

void OpenGLShader::Bind() const
{
	glUseProgram(m_RendererID);
}

void OpenGLShader::Unbind() const
{
	glUseProgram(0);
}

void OpenGLShader::upload_uniform_mat3(const std::string& name, const glm::mat4& matrix)
{
	int location = glGetUniformLocation(m_RendererID, name.c_str());
	glUniformMatrix3fv(location, 1, GL_FALSE, glm::value_ptr(matrix));
}

void OpenGLShader::upload_uniform_mat4(const std::string& name, const glm::mat4& matrix)
{
	int location = glGetUniformLocation(m_RendererID, name.c_str());
	glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(matrix));
}

void OpenGLShader::upload_uniform_int(const std::string& name, int value)
{
	int location = glGetUniformLocation(m_RendererID, name.c_str());
	glUniform1i(location, value);
}

void OpenGLShader::upload_uniform_float(const std::string& name, float value)
{
	int location = glGetUniformLocation(m_RendererID, name.c_str());
	glUniform1f(location, value);
}

void OpenGLShader::upload_uniform_float2(const std::string& name, const glm::vec2& vec)
{
	int location = glGetUniformLocation(m_RendererID, name.c_str());
	glUniform2f(location, vec.x, vec.y);
}

void OpenGLShader::upload_uniform_float3(const std::string& name, const glm::vec3& vec)
{
	int location = glGetUniformLocation(m_RendererID, name.c_str());
	glUniform3f(location, vec.r, vec.g, vec.b);
}

void OpenGLShader::upload_uniform_float4(const std::string& name, const glm::vec4& vec)
{
	int location = glGetUniformLocation(m_RendererID, name.c_str());
	glUniform4f(location, vec.r, vec.g, vec.b, vec.a);
}


_WHIP_END