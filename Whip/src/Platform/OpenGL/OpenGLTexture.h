#pragma once

#include <Whip/Render/Texture.h>

_WHIP_START

class opengl_texture2D : public texture2D
{
private:
	std::string	m_path;
	uint32_t m_width = 0;
	uint32_t m_height = 0;
	renderer_id_t m_rendererID = 0;
public:
	opengl_texture2D(const std::string& path);
	virtual ~opengl_texture2D();

	WHP_NODISCARD virtual uint32_t get_width() const override { return m_width; }
	WHP_NODISCARD virtual uint32_t get_height() const override { return m_height; }
	virtual void bind(uint32_t slot = 0) const override;
};

_WHIP_END