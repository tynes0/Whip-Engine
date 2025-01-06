#include "whippch.h"
#include "OpenGLTexture.h"

_WHIP_START

namespace utils
{
	static GLenum whip_image_format_to_gl_data_format(image_format format)
	{
		switch (format)
		{
		case image_format::RGB8:  return GL_RGB;
		case image_format::RGBA8: return GL_RGBA;
		}

		WHP_CORE_ASSERT(false);
		return 0;
	}

	static GLenum whip_image_format_to_gl_internal_format(image_format format)
	{
		switch (format)
		{
		case image_format::RGB8:  return GL_RGB8;
		case image_format::RGBA8: return GL_RGBA8;
		}

		WHP_CORE_ASSERT(false);
		return 0;
	}
}


opengl_texture2D::opengl_texture2D(const texture_specification& spesification, raw_buffer data)
	: m_specification(spesification)
{
	WHP_PROFILE_FUNCTION();

	m_internal_format = utils::whip_image_format_to_gl_internal_format(m_specification.format);
	m_data_format = utils::whip_image_format_to_gl_data_format(m_specification.format);

	glCreateTextures(GL_TEXTURE_2D, 1, &m_rendererID);
	glTextureStorage2D(m_rendererID, 1, m_internal_format, m_specification.width, m_specification.height);

	glTextureParameteri(m_rendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTextureParameteri(m_rendererID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTextureParameteri(m_rendererID, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTextureParameteri(m_rendererID, GL_TEXTURE_WRAP_T, GL_REPEAT);
	if (data)
		set_data(data);
}


opengl_texture2D::~opengl_texture2D()
{
	glDeleteTextures(1, &m_rendererID);
}

void opengl_texture2D::set_data(raw_buffer data)
{
	WHP_PROFILE_FUNCTION();

	uint32_t bytes_per_pixel = (m_data_format == GL_RGBA) ? 4 : 3;
	WHP_CORE_ASSERT(data.size == m_specification.width * m_specification.height * bytes_per_pixel, "Data must be entire texture!");

	glTextureSubImage2D(m_rendererID, 0, 0, 0, m_specification.width, m_specification.height, m_data_format, GL_UNSIGNED_BYTE, data.data);
}

raw_buffer opengl_texture2D::get_data() const
{
	WHP_PROFILE_FUNCTION();

	uint32_t bytes_per_pixel = (m_data_format == GL_RGBA) ? 4 : 3;
	uint64_t total_size = uint64_t(m_specification.width * m_specification.height * bytes_per_pixel);

	raw_buffer buffer(total_size);

	glBindTexture(GL_TEXTURE_2D, m_rendererID);
	glGetTexImage(GL_TEXTURE_2D, 0, m_data_format, GL_UNSIGNED_BYTE, buffer.data);
	glBindTexture(GL_TEXTURE_2D, 0);

	return buffer;
}

void opengl_texture2D::bind(uint32_t slot) const
{
	WHP_PROFILE_FUNCTION();

	glBindTextureUnit(slot, m_rendererID);
}

bool opengl_texture2D::operator==(const texture& other) const
{
	return (m_rendererID == other.get_renderer_id());
}

_WHIP_END
