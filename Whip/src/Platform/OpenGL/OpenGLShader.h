#pragma once

#include <Whip/Render/Shader.h>

#include <glm/glm.hpp>

//temp
typedef unsigned int GLenum;

_WHIP_START

class opengl_shader : public shader
{
private:
	renderer_id_t m_rendererID;
	std::string m_name;
private:
	WHP_NODISCARD std::unordered_map<GLenum, std::string> pre_process(const std::string& source);
	void compile(const std::unordered_map<GLenum, std::string>& shader_sources);
public:
	opengl_shader(const std::string& filepath);
	opengl_shader(const std::string& name, const std::string& filepath);
	opengl_shader(const std::string& name, const std::string& vertexSrc, const std::string& fragmentSrc, short _Test);
	opengl_shader(const std::string& name, const std::string& vertex_filepath, const std::string& fragment_filepath);
	virtual ~opengl_shader();

	WHP_NODISCARD virtual const std::string& get_name() const override { return m_name; }

	virtual void bind() const override;
	virtual void unbind() const override;

	virtual void set_int(const std::string& name, int value) override;
	virtual void set_int_array(const std::string& name, int* values, uint32_t count) override;
	virtual void set_float(const std::string& name, float value) override;
	virtual void set_float3(const std::string& name, const glm::vec3& value) override;
	virtual void set_float4(const std::string& name, const glm::vec4& value) override;
	virtual void set_mat4(const std::string& name, const glm::mat4& value) override;
	virtual void set_double(const std::string& name, double value) override;

	void upload_uniform_mat3(const std::string& name, const glm::mat3& matrix);
	void upload_uniform_mat4(const std::string& name, const glm::mat4& matrix);

	void upload_uniform_int(const std::string& name, int value);
	void upload_uniform_int_array(const std::string& name, int* values, uint32_t count);
	
	void upload_uniform_float(const std::string& name, float value);
	void upload_uniform_float2(const std::string& name, const glm::vec2& vec);
	void upload_uniform_float3(const std::string& name, const glm::vec3& vec);
	void upload_uniform_float4(const std::string& name, const glm::vec4& vec);

	void upload_uniform_double(const std::string& name, double value);
};

_WHIP_END