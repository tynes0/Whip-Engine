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

	static ref<Shader> create(const std::string& filepath);
	static ref<Shader> create(const std::string& vertex_filepath, const std::string& fragment_filepath);
	static ref<Shader> create(const std::string& vertex_src, const std::string& fragment_src, short test);
};

_WHIP_END