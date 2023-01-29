#pragma once

#include <Whip/Core.h>
#include <string>

_WHIP_START

class Shader
{
public:
	~Shader() = default;

	virtual void Bind() const = 0;
	virtual void Unbind() const = 0;

	static ref<Shader> create(const std::string& vertexSrc, const std::string& fragmentSrc);
};

_WHIP_END