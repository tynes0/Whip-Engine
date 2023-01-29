#pragma once

#include <Whip/Render/Shader.h>

#include <glm/glm.hpp>

_WHIP_START

class OpenGLShader : public Shader
{
private:
	renderer_id_t m_RendererID;
public:
	OpenGLShader(const std::string& vertexSrc, const std::string& fragmentSrc);
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