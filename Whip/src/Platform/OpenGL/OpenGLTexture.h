#pragma once

#include <Whip/Render/Texture.h>

_WHIP_START

class OpenGLTexture2D : public Texture2D
{
private:
	std::string	m_path;
	uint32_t m_width = 0;
	uint32_t m_height = 0;
	renderer_id_t m_rendererID = 0;
public:
	OpenGLTexture2D(const std::string& path);
	virtual ~OpenGLTexture2D();

	virtual uint32_t get_width() const override { return m_width; }
	virtual uint32_t get_height() const override { return m_height; }
	virtual void bind(uint32_t slot = 0) const override;
};

_WHIP_END