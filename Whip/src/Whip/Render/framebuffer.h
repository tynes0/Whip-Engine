#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/memory.h>

#include <vector>

_WHIP_START

enum class framebuffer_texture_format
{
	none = 0,
	RGBA8,
	RED_INTEGER,
	DEPTH24STENCIL8,
	depth = DEPTH24STENCIL8
};

struct framebuffer_texture_specification
{
	framebuffer_texture_specification() = default;
	framebuffer_texture_specification(framebuffer_texture_format format) : texture_format(format) {}

	framebuffer_texture_format texture_format = framebuffer_texture_format::none;
};

struct framebuffer_attachment_specification
{
	framebuffer_attachment_specification() = default;
	framebuffer_attachment_specification(std::initializer_list<framebuffer_texture_specification> attachments) : attachments(attachments) {}

	std::vector<framebuffer_texture_specification> attachments;
};

struct framebuffer_specification
{
	uint32_t width = 0;
	uint32_t height = 0;
	framebuffer_attachment_specification attachments;
	uint32_t samples = 1;

	bool swap_chain_target = false;
};


class framebuffer
{
public:
	virtual ~framebuffer() = default;

	virtual void bind() = 0;
	virtual void unbind() = 0;
	virtual void resize(uint32_t width, uint32_t height) = 0;
	virtual int read_pixel(uint32_t attachment_index, int x, int y) = 0;
	virtual void clear_attachment(uint32_t attachment_index, int value) = 0;
	virtual uint32_t get_color_attachment_renderer_id(uint32_t index = 0) const = 0;
	virtual const framebuffer_specification& get_specification() const = 0;

	static ref<framebuffer> create(const framebuffer_specification& spec);
};

_WHIP_END
