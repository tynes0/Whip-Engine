#pragma once

#include <Whip/Render/Texture.h>

#include <glad/glad.h>

_WHIP_START

class opengl_texture2D : public texture2D
{
public:
	opengl_texture2D(const texture_specification& spesification, raw_buffer data = raw_buffer());
	virtual ~opengl_texture2D();

	virtual const texture_specification& get_specification() const override { return m_specification; }

	WHP_NODISCARD virtual uint32_t get_width() const override { return m_specification.width; }
	WHP_NODISCARD virtual uint32_t get_height() const override { return m_specification.height; }

	WHP_NODISCARD virtual renderer_id_t get_renderer_id() const override { return m_rendererID; }

	virtual void set_data(raw_buffer data) override;
	virtual raw_buffer get_data() const override;
	virtual void bind(uint32_t slot = 0) const override;
	virtual bool is_loaded() const override { return m_is_loaded; }
	virtual bool operator==(const texture& other) const override;
private:
	texture_specification m_specification;

	bool m_is_loaded = false;
	renderer_id_t m_rendererID = 0;
	GLenum m_internal_format, m_data_format;
};

_WHIP_END
