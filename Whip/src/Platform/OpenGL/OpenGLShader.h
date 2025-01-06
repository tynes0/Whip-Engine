#pragma once

#include <Whip/Render/Shader.h>

#include <vector>

#include <glm/glm.hpp>

//temp
typedef unsigned int GLenum;

_WHIP_START

class opengl_shader : public shader
{
public:
	opengl_shader(const std::string& filepath);
	opengl_shader(const std::string& name, const std::string& filepath);
	opengl_shader(const std::string& name, const std::string& vertex_filepath, const std::string& fragment_filepath);
	virtual ~opengl_shader();

	WHP_NODISCARD virtual const std::string& get_name() const override { return m_name; }

	virtual void bind() const override;
	virtual void unbind() const override;

	virtual void set_int(const std::string& name, int value) override;
	virtual void set_int_array(const std::string& name, int* values, uint32_t count) override;
	virtual void set_float(const std::string& name, float value) override;
	virtual void set_float2(const std::string& name, const glm::vec2& value) override;
	virtual void set_float3(const std::string& name, const glm::vec3& value) override;
	virtual void set_float4(const std::string& name, const glm::vec4& value) override;
	virtual void set_mat4(const std::string& name, const glm::mat4& value) override;
	virtual void set_double(const std::string& name, double value) override;

	void upload_uniform_mat3(const std::string& name, const glm::mat3& matrix) const;
	void upload_uniform_mat4(const std::string& name, const glm::mat4& matrix) const;

	void upload_uniform_int(const std::string& name, int value) const;
	void upload_uniform_int_array(const std::string& name, int* values, uint32_t count) const;
	
	void upload_uniform_float(const std::string& name, float value) const;
	void upload_uniform_float2(const std::string& name, const glm::vec2& vec) const;
	void upload_uniform_float3(const std::string& name, const glm::vec3& vec) const;
	void upload_uniform_float4(const std::string& name, const glm::vec4& vec) const;

	void upload_uniform_double(const std::string& name, double value) const;
private:
	WHP_NODISCARD std::unordered_map<GLenum, std::string> pre_process(const std::string& source);

	void compile_or_get_vulkan_binaries(const std::unordered_map<GLenum, std::string>& shaderSources);
	void compile_or_get_opengl_binaries();
	void create_program();
	void reflect(GLenum stage, const std::vector<uint32_t>& shaderData);
private:
	renderer_id_t m_rendererID;
	std::string m_name;

	std::string m_filepath;
	std::unordered_map<GLenum, std::vector<uint32_t>> m_vulkanSPIRV;
	std::unordered_map<GLenum, std::vector<uint32_t>> m_openglSPIRV;

	std::unordered_map<GLenum, std::string> m_opengl_source_code;
};

_WHIP_END
