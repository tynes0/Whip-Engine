#pragma once

#include <Whip/Render/Texture.h>

#include <glad/glad.h>

_WHIP_START

class opengl_texture2D : public texture2D
{
private:
	std::string	m_path;
	uint32_t m_width;
	uint32_t m_height;
	renderer_id_t m_rendererID = 0;
	GLenum m_internal_format, m_data_format;
public:
	opengl_texture2D(uint32_t width, uint32_t height);
	opengl_texture2D(const std::string& path);
	virtual ~opengl_texture2D();

	WHP_NODISCARD virtual uint32_t get_width() const override { return m_width; }
	WHP_NODISCARD virtual uint32_t get_height() const override { return m_height; }
	virtual void set_data(void* data, uint32_t size) override;
	virtual void bind(uint32_t slot = 0) const override;

	virtual bool operator==(const texture& other) const override;
};

_WHIP_END