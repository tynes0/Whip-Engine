#include "whippch.h"
#include "OpenGLTexture.h"

#include <stb_image.h>

_WHIP_START


opengl_texture2D::opengl_texture2D(uint32_t width, uint32_t height)
	: m_width(width), m_height(height)
{
	WHP_PROFILE_FUNCTION();

	m_internal_format = GL_RGBA8;
	m_data_format = GL_RGBA;

	glCreateTextures(GL_TEXTURE_2D, 1, &m_rendererID);
	glTextureStorage2D(m_rendererID, 1, m_internal_format, m_width, m_height);

	glTextureParameteri(m_rendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTextureParameteri(m_rendererID, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glTextureParameteri(m_rendererID, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTextureParameteri(m_rendererID, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

opengl_texture2D::opengl_texture2D(const std::string& path)
	: m_path(path)
{
	WHP_PROFILE_FUNCTION();

	int width, height, channels;
	stbi_set_flip_vertically_on_load(1);
	stbi_uc* data;
	{
		WHP_PROFILE_SCOPE("stbi_load - opengl_texture2D::opengl_texture2D(const std::string&)");
		data = stbi_load(path.c_str(), &width, &height, &channels, 0);
	}
	WHP_CORE_ASSERT(data, "Failed to load image!");
	m_width = width;
	m_height = height;

	m_internal_format = 0;
	m_data_format = 0;
	if (channels == 4)
	{
		m_internal_format = GL_RGBA8;
		m_data_format = GL_RGBA;
	}
	else if (channels == 3)
	{
		m_internal_format = GL_RGB8;
		m_data_format = GL_RGB;
	}

	WHP_CORE_ASSERT(m_internal_format && m_data_format, "Format not supported!");

	glCreateTextures(GL_TEXTURE_2D, 1, &m_rendererID);
	glTextureStorage2D(m_rendererID, 1, m_internal_format, m_width, m_height);

	glTextureParameteri(m_rendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTextureParameteri(m_rendererID, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glTextureParameteri(m_rendererID, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTextureParameteri(m_rendererID, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glTextureSubImage2D(m_rendererID, 0, 0, 0, m_width, m_height, m_data_format, GL_UNSIGNED_BYTE, data);

	stbi_image_free(data);
}

opengl_texture2D::~opengl_texture2D()
{
	WHP_PROFILE_FUNCTION();

	glDeleteTextures(1, &m_rendererID);
}

void opengl_texture2D::set_data(void* data, uint32_t size)
{
	WHP_PROFILE_FUNCTION();

	uint32_t bytes_per_pixel = (m_data_format == GL_RGBA) ? 4 : 3;
	WHP_CORE_ASSERT(size == m_width * m_height * bytes_per_pixel, "Data must be entire texture!");

	glTextureSubImage2D(m_rendererID, 0, 0, 0, m_width, m_height, m_data_format, GL_UNSIGNED_BYTE, data);
}

void opengl_texture2D::bind(uint32_t slot) const
{
	WHP_PROFILE_FUNCTION();

	glBindTextureUnit(slot, m_rendererID);
}

bool opengl_texture2D::operator==(const texture& other) const
{
	return (m_rendererID == ((opengl_texture2D&)other).m_rendererID);
}

_WHIP_END