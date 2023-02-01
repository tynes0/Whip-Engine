#pragma once

#include <Whip/Render/Shader.h>

#include <glm/glm.hpp>

//temp
typedef unsigned int GLenum;

_WHIP_START

class OpenGLShader : public Shader
{
private:
	renderer_id_t m_RendererID;
private:
	std::string read_file(const std::string& filepath);
	std::unordered_map<GLenum, std::string> pre_process(const std::string& source);
	void compile(const std::unordered_map<GLenum, std::string>& shader_sources);
public:
	OpenGLShader(const std::string& filepath);
	OpenGLShader(const std::string& vertexSrc, const std::string& fragmentSrc, short _Test);
	OpenGLShader(const std::string& vertex_filepath, const std::string& fragment_filepath);
	virtual ~OpenGLShader();

	virtual void Bind() const override;
	virtual void Unbind() const override;

	void upload_uniform_mat3(const std::string& name, const glm::mat4& matrix);
	void upload_uniform_mat4(const std::string& name, const glm::mat4& matrix);

	void upload_uniform_int(const std::string& name, int value);
	
	void upload_uniform_float(const std::string& name, float value);
	void upload_uniform_float2(const std::string& name, const glm::vec2& vec);
	void upload_uniform_float3(const std::string& name, const glm::vec3& vec);
	void upload_uniform_float4(const std::string& name, const glm::vec4& vec);
};

_WHIP_END