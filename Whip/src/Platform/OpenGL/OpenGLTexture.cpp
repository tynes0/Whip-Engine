#include "whippch.h"
#include "OpenGLTexture.h"

#include <stb_image.h>

#include <glad/glad.h>

_WHIP_START

OpenGLTexture2D::OpenGLTexture2D(const std::string& path)
	: m_path(path)
{
	int width, height, channels;
	stbi_set_flip_vertically_on_load(1);
	stbi_uc* data = stbi_load(path.c_str(), &width, &height, &channels, 0);
	WHP_CORE_ASSERT(data, "Failed to load image!");
	m_width = width;
	m_height = height;

	GLenum internal_format = 0, data_format = 0;
	if (channels == 4)
	{
		internal_format = GL_RGBA8;
		data_format = GL_RGBA;
	}
	else if (channels == 3)
	{
		internal_format = GL_RGB8;
		data_format = GL_RGB;
	}

	WHP_CORE_ASSERT(internal_format && data_format, "Format not supported!");

	glCreateTextures(GL_TEXTURE_2D, 1, &m_rendererID);
	glTextureStorage2D(m_rendererID, 1, internal_format, m_width, m_height);

	glTextureParameteri(m_rendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTextureParameteri(m_rendererID, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glTextureSubImage2D(m_rendererID, 0, 0, 0, m_width, m_height, data_format, GL_UNSIGNED_BYTE, data);

	stbi_image_free(data);
}

OpenGLTexture2D::~OpenGLTexture2D()
{
	glDeleteTextures(1, &m_rendererID);
}

void OpenGLTexture2D::bind(uint32_t slot) const
{
	glBindTextureUnit(0, m_rendererID);
}

_WHIP_END