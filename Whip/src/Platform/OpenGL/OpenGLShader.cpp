#include <whippch.h>
#include "OpenGLShader.h"

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

// temporary
namespace filesystem
{
	std::string read_file(const std::string& filepath)
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
		return result;
	}
}

_WHIP_START

static GLenum shader_type_from_string(const std::string& type)
{
	if (type == "vertex")							return GL_VERTEX_SHADER;
	if (type == "fragment" || type == "pixel")		return GL_FRAGMENT_SHADER;

	WHP_CORE_ASSERT(false, "Unknown shader type '{0}'", type);
	return 0;
}


opengl_shader::opengl_shader(const std::string& filepath)
	:m_rendererID(0)
{
	WHP_PROFILE_FUNCTION();

	std::string source = filesystem::read_file(filepath);
	auto shader_sources = pre_process(source);
	compile(shader_sources);

	m_name = std::filesystem::path(filepath).stem().string();
}

opengl_shader::opengl_shader(const std::string& name, const std::string& filepath)
	: m_rendererID(0), m_name(name)
{
	WHP_PROFILE_FUNCTION();

	std::string source = filesystem::read_file(filepath);
	auto shader_sources = pre_process(source);
	compile(shader_sources);
}

opengl_shader::opengl_shader(const std::string& name, const std::string& vertex_filepath, const std::string& fragment_filepath)
	:m_rendererID(0), m_name(name)
{
	WHP_PROFILE_FUNCTION();

	std::string vertex_source = filesystem::read_file(vertex_filepath);
	std::string fragment_source = filesystem::read_file(fragment_filepath);
	std::unordered_map<GLenum, std::string> sources;
	sources[GL_VERTEX_SHADER] = vertex_source;
	sources[GL_FRAGMENT_SHADER] = fragment_source;
	compile(sources);
}

opengl_shader::opengl_shader(const std::string& name, const std::string& vertex_source, const std::string& fragment_source, short _Test)
	: m_rendererID(0), m_name(name)
{
	WHP_PROFILE_FUNCTION();

	std::unordered_map<GLenum, std::string> sources;
	sources[GL_VERTEX_SHADER] = vertex_source;
	sources[GL_FRAGMENT_SHADER] = fragment_source;
	compile(sources);
}

opengl_shader::~opengl_shader()
{
	WHP_PROFILE_FUNCTION();

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
		WHP_CORE_ASSERT(eol != std::string::npos, "Syntax Error!");
		size_t begin = pos + type_token_length + 1;
		std::string type = source.substr(begin, eol - begin);
		type.erase(std::remove(type.begin(), type.end(), ' '), type.end());
		WHP_CORE_ASSERT(shader_type_from_string(type), "Invalid shader type specifier!");
		size_t next_line_pos = source.find_first_not_of("\r\n", eol);
		pos = source.find(type_token, next_line_pos);
		shader_sources[shader_type_from_string(type)] = source.substr(next_line_pos, pos - (next_line_pos) == std::string::npos ? source.size() - 1 : pos - next_line_pos);
	}
	return shader_sources;
}

void opengl_shader::compile(const std::unordered_map<GLenum, std::string>& shader_sources)
{
	WHP_PROFILE_FUNCTION();

	renderer_id_t program = glCreateProgram();
	WHP_CORE_ASSERT(shader_sources.size() <= 2, "Whip Engine only supports 2 shaders for now!");
	std::array<GLenum, 2> gl_shader_IDs;
	uint32_t gl_shader_ID_index = 0;

#if _WHP_HAS_CPP_VERSION(17)
	for (auto& [type, source] : shader_sources)
#else
	for(auto& item : shader_sources)
#endif
	{
#if !_WHP_HAS_CPP_VERSION(17)
		auto& type = item.first;
		auto& source = item.second;
#endif
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
		gl_shader_IDs[gl_shader_ID_index++] = shader;
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

	m_rendererID = program;
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

void opengl_shader::upload_uniform_mat3(const std::string& name, const glm::mat3& matrix)
{
	int location = glGetUniformLocation(m_rendererID, name.c_str());
	glUniformMatrix3fv(location, 1, GL_FALSE, glm::value_ptr(matrix));
}

void opengl_shader::upload_uniform_mat4(const std::string& name, const glm::mat4& matrix)
{
	GLint location = glGetUniformLocation(m_rendererID, name.c_str());
	glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(matrix));
}

void opengl_shader::upload_uniform_int(const std::string& name, int value)
{
	int location = glGetUniformLocation(m_rendererID, name.c_str());
	glUniform1i(location, value);
}

void opengl_shader::upload_uniform_int_array(const std::string& name, int* values, uint32_t count)
{
	int location = glGetUniformLocation(m_rendererID, name.c_str());
	glUniform1iv(location, count, values);
}

void opengl_shader::upload_uniform_float(const std::string& name, float value)
{
	int location = glGetUniformLocation(m_rendererID, name.c_str());
	glUniform1f(location, value);
}

void opengl_shader::upload_uniform_float2(const std::string& name, const glm::vec2& vec)
{
	int location = glGetUniformLocation(m_rendererID, name.c_str());
	glUniform2f(location, vec.x, vec.y);
}

void opengl_shader::upload_uniform_float3(const std::string& name, const glm::vec3& vec)
{
	int location = glGetUniformLocation(m_rendererID, name.c_str());
	glUniform3f(location, vec.r, vec.g, vec.b);
}

void opengl_shader::upload_uniform_float4(const std::string& name, const glm::vec4& vec)
{
	int location = glGetUniformLocation(m_rendererID, name.c_str());
	glUniform4f(location, vec.r, vec.g, vec.b, vec.a);
}

void opengl_shader::upload_uniform_double(const std::string& name, double value)
{
	int location = glGetUniformLocation(m_rendererID, name.c_str());
	glUniform1d(location, value);
}


_WHIP_END