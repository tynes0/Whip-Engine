#pragma once

#include <Whip/Core.h>
#include <string>


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
};

_WHIP_END