#include <WhipPch.h>
#include "OpenGLShader.h"

#include <fstream>

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

#include <shaderc/shaderc.hpp>
#include <spirv_cross/spirv_cross.hpp>
#include <spirv_cross/spirv_glsl.hpp>

#include <Whip/Utils/Utility.h>

#include <coco.h>

_WHIP_START

namespace Utils
{

	static GLenum ShaderTypeFromString(const std::string& type)
	{
		if (type == "vertex")							return GL_VERTEX_SHADER;
		if (type == "fragment" || type == "pixel")		return GL_FRAGMENT_SHADER;

		WHP_CORE_ASSERT(false, "Unknown Shader type");
		return 0;
	}

	static shaderc_shader_kind GlShaderStageToShaderc(GLenum stage)
	{
		switch (stage)
		{
		case GL_VERTEX_SHADER:   return shaderc_glsl_vertex_shader;
		case GL_FRAGMENT_SHADER: return shaderc_glsl_fragment_shader;
		}
		WHP_CORE_ASSERT(false, "Unknown Shader type");
		return (shaderc_shader_kind)0;
	}

	static const char* GlShaderStageToString(GLenum stage)
	{
		switch (stage)
		{
		case GL_VERTEX_SHADER:   return "GL_VERTEX_SHADER";
		case GL_FRAGMENT_SHADER: return "GL_FRAGMENT_SHADER";
		}
		WHP_CORE_ASSERT(false, "Unknown Shader type");
		return nullptr;
	}

	static const char* GetCacheDirectory()
	{
		// TODO: make sure the assets directory is valid
		return "assets/cache/Shader/opengl";
	}

	static void CreateCacheDirectoryIfNeeded()
	{
		std::string cacheDirectory = GetCacheDirectory();
		if (!std::filesystem::exists(cacheDirectory))
			std::filesystem::create_directories(cacheDirectory);
	}

	static const char* GlShaderStageCachedOpenglFileExtension(uint32_t stage)
	{
		switch (stage)
		{
		case GL_VERTEX_SHADER:    return ".cached_opengl.vert";
		case GL_FRAGMENT_SHADER:  return ".cached_opengl.frag";
		}
		WHP_CORE_ASSERT(false, "Unknown Shader type");
		return "";
	}

	static const char* GlShaderStageCachedVulkanFileExtension(uint32_t stage)
	{
		switch (stage)
		{
		case GL_VERTEX_SHADER:    return ".cached_vulkan.vert";
		case GL_FRAGMENT_SHADER:  return ".cached_vulkan.frag";
		}
		WHP_CORE_ASSERT(false, "Unknown Shader type");
		return "";
	}
}

OpenGLShader::OpenGLShader(const std::string& filepath)
	:m_RendererID(0), m_Filepath(filepath)
{
	WHP_PROFILE_FUNCTION();

	std::string source = Utils::ReadFile(filepath);

	auto shaderSources = PreProcess(source);

	{
		coco::timer t;
		CompileOrGetVulkanBinaries(shaderSources);
		CompileOrGetOpenGLBinaries();
		CreateProgram();
		t.stop();
		WHP_CORE_WARN("[OpenGL Shader] Shader creation took {0} ms", t.get_casted_time<coco::time_units::milliseconds>());
	}

	m_Name = Utils::FetchFilename(filepath);
}

OpenGLShader::OpenGLShader(const std::string& name, const std::string& filepath)
	: m_RendererID(0), m_Name(name), m_Filepath(filepath)
{
	WHP_PROFILE_FUNCTION();

	std::string source = Utils::ReadFile(filepath);
	auto shaderSources = PreProcess(source);

	{
		coco::timer t;
		CompileOrGetVulkanBinaries(shaderSources);
		CompileOrGetOpenGLBinaries();
		CreateProgram();
		t.stop();
		WHP_CORE_WARN("[OpenGL Shader] Shader creation took {0} ms", t.get_casted_time<coco::time_units::milliseconds>());
	}
}

OpenGLShader::OpenGLShader(const std::string& name, const std::string& vertexFilepath, const std::string& fragmentFilepath)
	:m_RendererID(0), m_Name(name)
{
	WHP_PROFILE_FUNCTION();

	std::string vertexSource = Utils::ReadFile(vertexFilepath);
	std::string fragmentSource = Utils::ReadFile(fragmentFilepath);
	std::unordered_map<GLenum, std::string> sources;
	sources[GL_VERTEX_SHADER] = vertexSource;
	sources[GL_FRAGMENT_SHADER] = fragmentSource;

	{
		coco::timer t;
		CompileOrGetVulkanBinaries(sources);
		CompileOrGetOpenGLBinaries();
		CreateProgram();
		t.stop();
		WHP_CORE_WARN("[OpenGL Shader] Shader creation took {0} ms", t.get_casted_time<coco::time_units::milliseconds>());
	}
}

OpenGLShader::~OpenGLShader()
{
	glDeleteProgram(m_RendererID);
}

std::unordered_map<GLenum, std::string> OpenGLShader::PreProcess(const std::string& source)
{
	WHP_PROFILE_FUNCTION();

	std::unordered_map<GLenum, std::string> shaderSources;
	const char* typeToken = "#type";
	size_t typeTokenLength = strlen(typeToken);
	size_t pos = source.find(typeToken, 0);
	while (pos != std::string::npos)
	{
		size_t eol = source.find_first_of("\r\n", pos);
		WHP_CORE_ASSERT(eol != std::string::npos, "[OpenGL Shader] Syntax Error!");
		size_t begin = pos + typeTokenLength + 1;
		std::string type = source.substr(begin, eol - begin);
		type.erase(std::remove(type.begin(), type.end(), ' '), type.end());
		WHP_CORE_ASSERT(Utils::ShaderTypeFromString(type), "[OpenGL Shader] Invalid Shader type specifier!");
		size_t nextLinePos = source.find_first_not_of("\r\n", eol);
		pos = source.find(typeToken, nextLinePos);
		shaderSources[Utils::ShaderTypeFromString(type)] = source.substr(nextLinePos, pos - (nextLinePos) == std::string::npos ? source.size() - 1 : pos - nextLinePos);
	}
	return shaderSources;
}

void OpenGLShader::CompileOrGetVulkanBinaries(const std::unordered_map<GLenum, std::string>& shaderSources)
{
	RendererId program = glCreateProgram();

	shaderc::Compiler compiler;
	shaderc::CompileOptions options;
	options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);
	const bool optimize = true;
	if (optimize)
		options.SetOptimizationLevel(shaderc_optimization_level_performance);
	std::filesystem::path cacheDirectory = Utils::GetCacheDirectory();
	auto& shaderData = m_VulkanSPIRV;
	shaderData.clear();
	for (auto&& [stage, source] : shaderSources)
	{
		std::filesystem::path shaderFilePath = m_Filepath;
		std::filesystem::path cachedPath = cacheDirectory / (shaderFilePath.filename().string() + Utils::GlShaderStageCachedVulkanFileExtension(stage));

		std::ifstream in(cachedPath, std::ios::in | std::ios::binary);
		if (in.is_open())
		{
			in.seekg(0, std::ios::end);
			auto size = in.tellg();
			in.seekg(0, std::ios::beg);
			auto& data = shaderData[stage];
			data.resize(size / sizeof(uint32_t));
			in.read((char*)data.data(), size);
		}
		else
		{
			shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(source, Utils::GlShaderStageToShaderc(stage), m_Filepath.c_str(), options);
			if (module.GetCompilationStatus() != shaderc_compilation_status_success)
			{
				WHP_CORE_ERROR(module.GetErrorMessage());
				WHP_CORE_ASSERT(false, "");
			}

			shaderData[stage] = std::vector<uint32_t>(module.cbegin(), module.cend());

			std::ofstream out(cachedPath, std::ios::out | std::ios::binary);
			if (out.is_open())
			{
				auto& data = shaderData[stage];
				out.write((char*)data.data(), data.size() * sizeof(uint32_t));
				out.flush();
				out.close();
			}
		}
		for (auto&& [shaderStage, shaderStageData] : shaderData)
			Reflect(shaderStage, shaderStageData);
	}
}

void OpenGLShader::CompileOrGetOpenGLBinaries()
{
	auto& shaderData = m_OpenGLSPIRV;

	shaderc::Compiler compiler;
	shaderc::CompileOptions options;
	options.SetTargetEnvironment(shaderc_target_env_opengl, shaderc_env_version_opengl_4_5);
	const bool optimize = false;
	if (optimize)
		options.SetOptimizationLevel(shaderc_optimization_level_performance);

	std::filesystem::path cacheDirectory = Utils::GetCacheDirectory();

	shaderData.clear();
	m_OpenGLSourceCode.clear();
	for (auto&& [stage, spirv] : m_VulkanSPIRV)
	{
		std::filesystem::path shaderFilePath = m_Filepath;
		std::filesystem::path cachedPath = cacheDirectory / (shaderFilePath.filename().string() + Utils::GlShaderStageCachedOpenglFileExtension(stage));

		std::ifstream in(cachedPath, std::ios::in | std::ios::binary);
		if (in.is_open())
		{
			in.seekg(0, std::ios::end);
			auto size = in.tellg();
			in.seekg(0, std::ios::beg);

			auto& data = shaderData[stage];
			data.resize(size / sizeof(uint32_t));
			in.read((char*)data.data(), size);
		}
		else
		{
			spirv_cross::CompilerGLSL glslCompiler(spirv);
			m_OpenGLSourceCode[stage] = glslCompiler.compile();
			auto& source = m_OpenGLSourceCode[stage];

			shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(source, Utils::GlShaderStageToShaderc(stage), m_Filepath.c_str());
			if (module.GetCompilationStatus() != shaderc_compilation_status_success)
			{
				WHP_CORE_ERROR(module.GetErrorMessage());
				WHP_CORE_ASSERT(false, "");
			}

			shaderData[stage] = std::vector<uint32_t>(module.cbegin(), module.cend());

			std::ofstream out(cachedPath, std::ios::out | std::ios::binary);
			if (out.is_open())
			{
				auto& data = shaderData[stage];
				out.write((char*)data.data(), data.size() * sizeof(uint32_t));
				out.flush();
				out.close();
			}
		}
	}
}

void OpenGLShader::CreateProgram()
{
	GLuint program = glCreateProgram();

	std::vector<GLuint> shaderIDs;
	for (auto&& [stage, spirv] : m_OpenGLSPIRV)
	{
		GLuint shaderID = shaderIDs.emplace_back(glCreateShader(stage));
		glShaderBinary(1, &shaderID, GL_SHADER_BINARY_FORMAT_SPIR_V, spirv.data(), (GLsizei)spirv.size() * sizeof(uint32_t));
		glSpecializeShader(shaderID, "main", 0, nullptr, nullptr);
		glAttachShader(program, shaderID);
	}

	glLinkProgram(program);

	GLint isLinked;
	glGetProgramiv(program, GL_LINK_STATUS, &isLinked);
	if (isLinked == GL_FALSE)
	{
		GLint maxLength;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);

		std::vector<GLchar> infoLog(maxLength);
		glGetProgramInfoLog(program, maxLength, &maxLength, infoLog.data());
		WHP_CORE_ERROR("[OpenGL Shader] Shader linking failed ({0}):\n{1}", m_Filepath, infoLog.data());

		glDeleteProgram(program);

		for (auto id : shaderIDs)
			glDeleteShader(id);
	}

	for (auto id : shaderIDs)
	{
		glDetachShader(program, id);
		glDeleteShader(id);
	}

	m_RendererID = program;
}

void OpenGLShader::Reflect(GLenum stage, const std::vector<uint32_t>& shaderData)
{
	spirv_cross::Compiler compiler(shaderData);
	spirv_cross::ShaderResources resources = compiler.get_shader_resources();

	WHP_CORE_TRACE("[OpenGL Shader] Reflect - {0} {1}", Utils::GlShaderStageToString(stage), m_Filepath);

	for (const auto& resource : resources.uniform_buffers)
	{
		const auto& bufferType = compiler.get_type(resource.base_type_id);
		uint32_t bufferSize = (uint32_t)compiler.get_declared_struct_size(bufferType);
		uint32_t binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
		int memberCount = (int)bufferType.member_types.size();
	}
}

void OpenGLShader::Bind() const
{
	WHP_PROFILE_FUNCTION();

	glUseProgram(m_RendererID);
}

void OpenGLShader::Unbind() const
{
	WHP_PROFILE_FUNCTION();

	glUseProgram(0);
}

void OpenGLShader::SetInt(const std::string& name, int value)
{
	WHP_PROFILE_FUNCTION();

	UploadUniformInt(name, value);
}

void OpenGLShader::SetIntArray(const std::string& name, int* values, uint32_t count)
{
	WHP_PROFILE_FUNCTION();

	UploadUniformIntArray(name, values, count);
}

void OpenGLShader::SetFloat(const std::string& name, float value)
{
	WHP_PROFILE_FUNCTION();

	UploadUniformFloat(name, value);
}

void OpenGLShader::SetFloat2(const std::string& name, const glm::vec2& value)
{
	WHP_PROFILE_FUNCTION();

	UploadUniformFloat2(name, value);
}

void OpenGLShader::SetFloat3(const std::string& name, const glm::vec3& value)
{
	WHP_PROFILE_FUNCTION();

	UploadUniformFloat3(name, value);
}

void OpenGLShader::SetFloat4(const std::string& name, const glm::vec4& value)
{
	WHP_PROFILE_FUNCTION();

	UploadUniformFloat4(name, value);
}

void OpenGLShader::SetMat4(const std::string& name, const glm::mat4& value)
{
	WHP_PROFILE_FUNCTION();

	UploadUniformMat4(name, value);
}

void OpenGLShader::SetDouble(const std::string& name, double value)
{
	WHP_PROFILE_FUNCTION();

	UploadUniformDouble(name, value);
}

void OpenGLShader::UploadUniformMat3(const std::string& name, const glm::mat3& matrix) const
{
	int location = glGetUniformLocation(m_RendererID, name.c_str());
	glUniformMatrix3fv(location, 1, GL_FALSE, glm::value_ptr(matrix));
}

void OpenGLShader::UploadUniformMat4(const std::string& name, const glm::mat4& matrix) const
{
	GLint location = glGetUniformLocation(m_RendererID, name.c_str());
	glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(matrix));
}

void OpenGLShader::UploadUniformInt(const std::string& name, int value) const
{
	int location = glGetUniformLocation(m_RendererID, name.c_str());
	glUniform1i(location, value);
}

void OpenGLShader::UploadUniformIntArray(const std::string& name, int* values, uint32_t count) const
{
	int location = glGetUniformLocation(m_RendererID, name.c_str());
	glUniform1iv(location, count, values);
}

void OpenGLShader::UploadUniformFloat(const std::string& name, float value) const
{
	int location = glGetUniformLocation(m_RendererID, name.c_str());
	glUniform1f(location, value);
}

void OpenGLShader::UploadUniformFloat2(const std::string& name, const glm::vec2& vec) const
{
	int location = glGetUniformLocation(m_RendererID, name.c_str());
	glUniform2f(location, vec.x, vec.y);
}

void OpenGLShader::UploadUniformFloat3(const std::string& name, const glm::vec3& vec) const
{
	int location = glGetUniformLocation(m_RendererID, name.c_str());
	glUniform3f(location, vec.r, vec.g, vec.b);
}

void OpenGLShader::UploadUniformFloat4(const std::string& name, const glm::vec4& vec) const
{
	int location = glGetUniformLocation(m_RendererID, name.c_str());
	glUniform4f(location, vec.r, vec.g, vec.b, vec.a);
}

void OpenGLShader::UploadUniformDouble(const std::string& name, double value) const
{
	int location = glGetUniformLocation(m_RendererID, name.c_str());
	glUniform1d(location, value);
}


_WHIP_END
