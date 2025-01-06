#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/memory.h>
#include <string>
#include <unordered_map>

#include <glm/glm.hpp>

_WHIP_START

class shader
{
public:
	~shader() = default;

	virtual const std::string& get_name() const = 0;

	virtual void bind() const = 0;
	virtual void unbind() const = 0;

	virtual void set_int(const std::string& name, int value) = 0;
	virtual void set_int_array(const std::string& name, int* values, uint32_t count) = 0;
	virtual void set_float(const std::string& name, float value) = 0;
	virtual void set_float2(const std::string& name, const glm::vec2& value) = 0;
	virtual void set_float3(const std::string& name, const glm::vec3& value) = 0;
	virtual void set_float4(const std::string& name, const glm::vec4& value) = 0;
	virtual void set_mat4(const std::string& name, const glm::mat4& value) = 0;
	virtual void set_double(const std::string& name, double value) = 0;

	WHP_NODISCARD static ref<shader> create(const std::string& filepath);
	WHP_NODISCARD static ref<shader> create(const std::string& name, const std::string& filepath);
	WHP_NODISCARD static ref<shader> create(const std::string& name, const std::string& vertex_filepath, const std::string& fragment_filepath);
};

class shader_library
{
public:
	void add(const std::string& name, const ref<shader>& shader);
	void add(const ref<shader>& shader);
	ref<shader> load(const std::string& filepath);
	ref<shader> load(const std::string& name, const std::string& filepath);
	ref<shader> load(const std::string& name, const std::string& vertex_filepath, const std::string& fragment_filepath);

	WHP_NODISCARD ref<shader> get(const std::string& name);

	WHP_NODISCARD bool exist(const std::string& name) const;
private:
	std::unordered_map<std::string, ref<shader>> m_shaders;
};

_WHIP_END
