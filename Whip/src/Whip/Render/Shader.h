#pragma once

#include <Whip/Core.h>
#include <string>

#include <glm/gtc/type_ptr.hpp>


_WHIP_START

class Shader
{
private:
	renderer_id_t m_RendererID;
public:
	Shader(const std::string& vertexSrc, const std::string& fragmentSrc);
	~Shader();

	void Bind() const;
	void Unbind() const;

	void upload_uniform_mat4(const std::string& name, const glm::mat4& matrix);
};

_WHIP_END