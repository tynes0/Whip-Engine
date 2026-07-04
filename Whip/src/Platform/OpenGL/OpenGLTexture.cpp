#include "WhipPch.h"
#include "OpenGLTexture.h"

_WHIP_START

namespace Utils
{
	static GLenum WhipImageFormatToGLDataFormat(ImageFormat format)
	{
		switch (format)
		{
		case ImageFormat::Rgb8:  return GL_RGB;
		case ImageFormat::Rgba8: return GL_RGBA;
		}

		WHP_CORE_ASSERT(false);
		return 0;
	}

	static GLenum WhipImageFormatToGLInternalFormat(ImageFormat format)
	{
		switch (format)
		{
		case ImageFormat::Rgb8:  return GL_RGB8;
		case ImageFormat::Rgba8: return GL_RGBA8;
		}

		WHP_CORE_ASSERT(false);
		return 0;
	}

	static GLenum WhipTextureFilterToGLMagFilter(TextureFilterMode mode)
	{
		switch (mode)
		{
		case TextureFilterMode::Nearest: return GL_NEAREST;
		case TextureFilterMode::Linear: return GL_LINEAR;
		default: return GL_LINEAR;
		}
	}

	static GLenum WhipTextureFilterToGLMinFilter(TextureFilterMode mode, bool generateMips)
	{
		if (!generateMips)
			return WhipTextureFilterToGLMagFilter(mode);

		switch (mode)
		{
		case TextureFilterMode::Nearest: return GL_NEAREST_MIPMAP_NEAREST;
		case TextureFilterMode::Linear: return GL_LINEAR_MIPMAP_LINEAR;
		default: return GL_LINEAR_MIPMAP_LINEAR;
		}
	}

	static GLenum WhipTextureWrapToGLWrap(TextureWrapMode mode)
	{
		switch (mode)
		{
		case TextureWrapMode::Repeat: return GL_REPEAT;
		case TextureWrapMode::ClampToEdge: return GL_CLAMP_TO_EDGE;
		default: return GL_REPEAT;
		}
	}

	static uint32_t CalculateMipLevelCount(uint32_t width, uint32_t height)
	{
		uint32_t levels = 1;
		uint32_t size = std::max(width, height);
		while (size > 1)
		{
			size /= 2;
			++levels;
		}
		return levels;
	}
}


OpenGLTexture2D::OpenGLTexture2D(const TextureSpecification& specification, RawBuffer data)
	: m_Specification(specification)
{
	WHP_PROFILE_FUNCTION();

	m_InternalFormat = Utils::WhipImageFormatToGLInternalFormat(m_Specification.m_Format);
	m_DataFormat = Utils::WhipImageFormatToGLDataFormat(m_Specification.m_Format);
	const uint32_t mipLevels = m_Specification.m_GenerateMips ? Utils::CalculateMipLevelCount(m_Specification.m_Width, m_Specification.m_Height) : 1;

	glCreateTextures(GL_TEXTURE_2D, 1, &m_RendererID);
	glTextureStorage2D(m_RendererID, mipLevels, m_InternalFormat, m_Specification.m_Width, m_Specification.m_Height);

	glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, Utils::WhipTextureFilterToGLMinFilter(m_Specification.m_FilterMode, m_Specification.m_GenerateMips));
	glTextureParameteri(m_RendererID, GL_TEXTURE_MAG_FILTER, Utils::WhipTextureFilterToGLMagFilter(m_Specification.m_FilterMode));

	const GLenum wrapMode = Utils::WhipTextureWrapToGLWrap(m_Specification.m_WrapMode);
	glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_S, wrapMode);
	glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_T, wrapMode);
	if (data)
		SetData(data);
}


OpenGLTexture2D::~OpenGLTexture2D()
{
	glDeleteTextures(1, &m_RendererID);
}

void OpenGLTexture2D::SetData(RawBuffer data)
{
	WHP_PROFILE_FUNCTION();

	uint32_t bytesPerPixel = (m_DataFormat == GL_RGBA) ? 4 : 3;
	WHP_CORE_ASSERT(data.m_Size == m_Specification.m_Width * m_Specification.m_Height * bytesPerPixel, "Data must be entire Texture!");

	glTextureSubImage2D(m_RendererID, 0, 0, 0, m_Specification.m_Width, m_Specification.m_Height, m_DataFormat, GL_UNSIGNED_BYTE, data.m_Data);
	if (m_Specification.m_GenerateMips)
		glGenerateTextureMipmap(m_RendererID);
	m_IsLoaded = true;
}

RawBuffer OpenGLTexture2D::GetData() const
{
	WHP_PROFILE_FUNCTION();

	uint32_t bytesPerPixel = (m_DataFormat == GL_RGBA) ? 4 : 3;
	uint64_t totalSize = uint64_t(m_Specification.m_Width * m_Specification.m_Height * bytesPerPixel);

	RawBuffer buffer(totalSize);

	glBindTexture(GL_TEXTURE_2D, m_RendererID);
	glGetTexImage(GL_TEXTURE_2D, 0, m_DataFormat, GL_UNSIGNED_BYTE, buffer.m_Data);
	glBindTexture(GL_TEXTURE_2D, 0);

	return buffer;
}

void OpenGLTexture2D::Bind(uint32_t slot) const
{
	WHP_PROFILE_HOT_FUNCTION();

	glBindTextureUnit(slot, m_RendererID);
}

bool OpenGLTexture2D::operator==(const Texture& other) const
{
	return (m_RendererID == other.GetRendererId());
}

_WHIP_END
