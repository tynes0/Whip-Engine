#pragma once

#include <Whip/Render/Shader.h>

#include <vector>

#include <glm/glm.hpp>

//temp
typedef unsigned int GLenum;

_WHIP_START

class OpenGLShader : public Shader
{
public:
	OpenGLShader(const std::string& filepath);
	OpenGLShader(const std::string& name, const std::string& filepath);
	OpenGLShader(const std::string& name, const std::string& vertexFilepath, const std::string& fragmentFilepath);
	virtual ~OpenGLShader();

	WHP_NODISCARD virtual const std::string& GetName() const override { return m_Name; }

	virtual void Bind() const override;
	virtual void Unbind() const override;

	virtual void SetInt(const std::string& name, int value) override;
	virtual void SetIntArray(const std::string& name, int* values, uint32_t count) override;
	virtual void SetFloat(const std::string& name, float value) override;
	virtual void SetFloat2(const std::string& name, const glm::vec2& value) override;
	virtual void SetFloat3(const std::string& name, const glm::vec3& value) override;
	virtual void SetFloat4(const std::string& name, const glm::vec4& value) override;
	virtual void SetMat4(const std::string& name, const glm::mat4& value) override;
	virtual void SetDouble(const std::string& name, double value) override;

	void UploadUniformMat3(const std::string& name, const glm::mat3& matrix) const;
	void UploadUniformMat4(const std::string& name, const glm::mat4& matrix) const;

	void UploadUniformInt(const std::string& name, int value) const;
	void UploadUniformIntArray(const std::string& name, int* values, uint32_t count) const;
	
	void UploadUniformFloat(const std::string& name, float value) const;
	void UploadUniformFloat2(const std::string& name, const glm::vec2& vec) const;
	void UploadUniformFloat3(const std::string& name, const glm::vec3& vec) const;
	void UploadUniformFloat4(const std::string& name, const glm::vec4& vec) const;

	void UploadUniformDouble(const std::string& name, double value) const;
private:
	WHP_NODISCARD std::unordered_map<GLenum, std::string> PreProcess(const std::string& source);

	void CompileOrGetVulkanBinaries(const std::unordered_map<GLenum, std::string>& shaderSources);
	void CompileOrGetOpenGLBinaries();
	void CreateProgram();
	void Reflect(GLenum stage, const std::vector<uint32_t>& shaderData);
private:
	RendererId m_RendererID;
	std::string m_Name;

	std::string m_Filepath;
	std::unordered_map<GLenum, std::vector<uint32_t>> m_VulkanSPIRV;
	std::unordered_map<GLenum, std::vector<uint32_t>> m_OpenGLSPIRV;

	std::unordered_map<GLenum, std::string> m_OpenGLSourceCode;
};

_WHIP_END
