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
}


OpenGLTexture2D::OpenGLTexture2D(const TextureSpecification& specification, RawBuffer data)
	: m_Specification(specification)
{
	WHP_PROFILE_FUNCTION();

	m_InternalFormat = Utils::WhipImageFormatToGLInternalFormat(m_Specification.m_Format);
	m_DataFormat = Utils::WhipImageFormatToGLDataFormat(m_Specification.m_Format);

	glCreateTextures(GL_TEXTURE_2D, 1, &m_RendererID);
	glTextureStorage2D(m_RendererID, 1, m_InternalFormat, m_Specification.m_Width, m_Specification.m_Height);

	glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTextureParameteri(m_RendererID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_T, GL_REPEAT);
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
	WHP_PROFILE_FUNCTION();

	glBindTextureUnit(slot, m_RendererID);
}

bool OpenGLTexture2D::operator==(const Texture& other) const
{
	return (m_RendererID == other.GetRendererId());
}

_WHIP_END
