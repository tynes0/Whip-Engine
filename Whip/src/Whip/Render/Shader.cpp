#include "whippch.h"
#include "Shader.h"

#include <Glad/glad.h>

_WHIP_START

Shader::Shader(const std::string& vertexSrc, const std::string& fragmentSrc)
{
	// create an empty vertex shader handle
	renderer_id_t vertexShader = glCreateShader(GL_VERTEX_SHADER);

	// send the vertex shader source code to GL
	// note that std::string's .c_str is NULL character terminated.
	const char* source = vertexSrc.c_str();
	glShaderSource(vertexShader, 1, &source, 0);

	// compile vertex shader
	glCompileShader(vertexShader);

	int is_compiled = 0;

	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &is_compiled);
	if (is_compiled == GL_FALSE)
	{
		int max_length = 0;
		glGetShaderiv(vertexShader, GL_INFO_LOG_LENGTH, &max_length);

		// the max_length includes the NULL characters
		std::vector<char> info_log(max_length);
		glGetShaderInfoLog(vertexShader, max_length, &max_length, &info_log[0]);

		// We don't need the shader anymore.
		glDeleteShader(vertexShader);

		WHP_CORE_ERROR("{0}", info_log.data());
		WHP_CORE_ASSERT(false, "Vertex shader compilation failure!");
		return;
	}
	// create an empty fragment shader handle
	renderer_id_t fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

	// send the vertex fragment source code to GL
	// note that std::string's .c_str is NULL character terminated.
	source = fragmentSrc.c_str();
	glShaderSource(fragmentShader, 1, &source, 0);

	// compile the fragment shader
	glCompileShader(fragmentShader);


	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &is_compiled);
	if (is_compiled == GL_FALSE)
	{
		int max_length = 0;
		glGetShaderiv(fragmentShader, GL_INFO_LOG_LENGTH, &max_length);

		// the max_length includes the NULL characters
		std::vector<char> info_log(max_length);
		glGetShaderInfoLog(fragmentShader, max_length, &max_length, &info_log[0]);

		// We don't need the shader anymore.
		glDeleteShader(fragmentShader);
		// Either of them. Don't leak shaders
		glDeleteShader(vertexShader);

		WHP_CORE_ERROR("{0}", info_log.data());
		WHP_CORE_ASSERT(false, "Fragment shader compilation failure!");
		return;
	}

	// vertex and fragment shaders are successfully compiled.
	// Now time to link them together into a program.
	// Get a program object.
	
	m_RendererID = glCreateProgram();
	// Attach our shader to our program
	glAttachShader(m_RendererID, vertexShader);
	glAttachShader(m_RendererID, fragmentShader);

	// Link our program
	glLinkProgram(m_RendererID);

	// Note the different functions here: glGetProgram* instead of glGetShader*
	int is_linked = 0;
	glGetProgramiv(m_RendererID, GL_LINK_STATUS, &is_linked);
	if (is_linked == GL_FALSE)
	{
		int max_length = 0;
		glGetProgramiv(m_RendererID, GL_INFO_LOG_LENGTH, &max_length);

		std::vector<char> info_log(max_length);
		glGetProgramInfoLog(m_RendererID, max_length, &max_length, &info_log[0]);

		// We don't need the program anymore
		glDeleteProgram(m_RendererID);

		// don't leak shader either.
		glDeleteShader(vertexShader);
		glDeleteShader(fragmentShader);

		WHP_CORE_ERROR("{0}", info_log.data());
		WHP_CORE_ASSERT(false, "Shader link failure!");
		return;
	}

	// Always detach shaders after successful link.
	glDetachShader(m_RendererID, vertexShader);
	glDetachShader(m_RendererID, fragmentShader);
}

Shader::~Shader()
{
	glDeleteProgram(m_RendererID);
}

void Shader::Bind() const
{
	glUseProgram(m_RendererID);
}

void Shader::Unbind() const
{
	glUseProgram(0);
}


_WHIP_END