#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/Memory.h>
#include <string>
#include <unordered_map>

#include <glm/glm.hpp>

_WHIP_START

class Shader
{
public:
	~Shader() = default;

	virtual const std::string& GetName() const = 0;

	virtual void Bind() const = 0;
	virtual void Unbind() const = 0;

	virtual void SetInt(const std::string& name, int value) = 0;
	virtual void SetIntArray(const std::string& name, int* values, uint32_t count) = 0;
	virtual void SetFloat(const std::string& name, float value) = 0;
	virtual void SetFloat2(const std::string& name, const glm::vec2& value) = 0;
	virtual void SetFloat3(const std::string& name, const glm::vec3& value) = 0;
	virtual void SetFloat4(const std::string& name, const glm::vec4& value) = 0;
	virtual void SetMat4(const std::string& name, const glm::mat4& value) = 0;
	virtual void SetDouble(const std::string& name, double value) = 0;

	WHP_NODISCARD static Ref<Shader> Create(const std::string& filepath);
	WHP_NODISCARD static Ref<Shader> Create(const std::string& name, const std::string& filepath);
	WHP_NODISCARD static Ref<Shader> Create(const std::string& name, const std::string& vertexFilepath, const std::string& fragmentFilepath);
};

class ShaderLibrary
{
public:
	void Add(const std::string& name, const Ref<Shader>& shader);
	void Add(const Ref<Shader>& shader);
	Ref<Shader> Load(const std::string& filepath);
	Ref<Shader> Load(const std::string& name, const std::string& filepath);
	Ref<Shader> Load(const std::string& name, const std::string& vertexFilepath, const std::string& fragmentFilepath);

	WHP_NODISCARD Ref<Shader> Get(const std::string& name);

	WHP_NODISCARD bool Exist(const std::string& name) const;
private:
	std::unordered_map<std::string, Ref<Shader>> m_Shaders;
};

_WHIP_END
