#pragma once
#include <Whip/Render/framebuffer.h>

_WHIP_START

class opengl_framebuffer : public framebuffer
{
public:
	opengl_framebuffer(const framebuffer_specification& spec);
	~opengl_framebuffer();

	void invalidate();
	void flush();

	virtual void bind() override;
	virtual void unbind() override;
	virtual void resize(uint32_t width, uint32_t height) override;
	virtual int read_pixel(uint32_t attachment_index, int x, int y) override;
	virtual void clear_attachment(uint32_t attachment_index, int value) override;
	virtual const framebuffer_specification& get_specification() const override { return m_spec; }
	virtual uint32_t get_color_attachment_renderer_id(uint32_t index = 0) const override { WHP_CORE_ASSERT(index < m_color_attachments.size(), "color attachment index out of the range"); return m_color_attachments[index]; }
private:
	renderer_id_t m_renderer_id = 0;
	framebuffer_specification m_spec;
	std::vector<framebuffer_texture_specification> m_color_attachment_specifications;
	framebuffer_texture_specification m_depth_attachment_specification = framebuffer_texture_format::none;
	std::vector<uint32_t> m_color_attachments;
	uint32_t m_depth_attachment = 0;
};

_WHIP_END
