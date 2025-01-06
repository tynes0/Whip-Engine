#include "whippch.h"
#include "opengl_framebuffer.h"

#include <glad/glad.h>

_WHIP_START

static constexpr uint32_t s_max_framebuffer_size = 8192;

namespace utils 
{

	static GLenum texture_target(bool multisampled)
	{
		return multisampled ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
	}

	static void create_textures(bool multisampled, uint32_t* outID, uint32_t count)
	{
		glCreateTextures(texture_target(multisampled), count, outID);
	}

	static void bind_texture(bool multisampled, uint32_t id)
	{
		glBindTexture(texture_target(multisampled), id);
	}

	static void attach_color_texture(uint32_t id, int samples, GLenum internal_format, GLenum format, uint32_t width, uint32_t height, int index)
	{
		bool multisampled = samples > 1;
		if (multisampled)
		{
			glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples, internal_format, width, height, GL_FALSE);
		}
		else
		{
			glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, format, GL_UNSIGNED_BYTE, nullptr);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		}

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + index, texture_target(multisampled), id, 0);
	}

	static void attach_depth_texture(uint32_t id, int samples, GLenum format, GLenum attachmentType, uint32_t width, uint32_t height)
	{
		bool multisampled = samples > 1;
		if (multisampled)
			glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples, format, width, height, GL_FALSE);
		else
		{
			glTexStorage2D(GL_TEXTURE_2D, 1, format, width, height);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		}

		glFramebufferTexture2D(GL_FRAMEBUFFER, attachmentType, texture_target(multisampled), id, 0);
	}

	static bool is_depth_format(framebuffer_texture_format format)
	{
		switch (format)
		{
		case framebuffer_texture_format::DEPTH24STENCIL8:  return true;
		}

		return false;
	}

	static GLenum whip_fb_texture_format_to_gl(framebuffer_texture_format format)
	{
		switch (format)
		{
		case framebuffer_texture_format::RGBA8:       return GL_RGBA8;
		case framebuffer_texture_format::RED_INTEGER: return GL_RED_INTEGER;
		}

		WHP_CORE_ASSERT(false, "Invalid texture format!");
		return 0;
	}
}

opengl_framebuffer::opengl_framebuffer(const framebuffer_specification& spec) : m_spec(spec)
{
	for (auto spec : m_spec.attachments.attachments)
	{
		if (!utils::is_depth_format(spec.texture_format))
			m_color_attachment_specifications.emplace_back(spec);
		else
			m_depth_attachment_specification = spec;
	}

	invalidate();
}

opengl_framebuffer::~opengl_framebuffer()
{
	flush();
}

void opengl_framebuffer::invalidate()
{
	if (m_renderer_id)
	{
		flush();
		m_color_attachments.clear();
		m_depth_attachment = 0;
	}

	glCreateFramebuffers(1, &m_renderer_id);
	glBindFramebuffer(GL_FRAMEBUFFER, m_renderer_id);

	bool multisample = m_spec.samples > 1;

	// Attachments
	if (m_color_attachment_specifications.size())
	{
		m_color_attachments.resize(m_color_attachment_specifications.size());
		utils::create_textures(multisample, m_color_attachments.data(), static_cast<uint32_t>(m_color_attachments.size()));

		for (size_t i = 0; i < m_color_attachments.size(); i++)
		{
			utils::bind_texture(multisample, m_color_attachments[i]);
			switch (m_color_attachment_specifications[i].texture_format)
			{
			case framebuffer_texture_format::RGBA8:
				utils::attach_color_texture(m_color_attachments[i], m_spec.samples, GL_RGBA8, GL_RGBA, m_spec.width, m_spec.height, static_cast<int>(i));
				break;
			case framebuffer_texture_format::RED_INTEGER:
				utils::attach_color_texture(m_color_attachments[i], m_spec.samples, GL_R32I, GL_RED_INTEGER, m_spec.width, m_spec.height, static_cast<int>(i));
				break;
			}
		}
	}

	if (m_depth_attachment_specification.texture_format != framebuffer_texture_format::none)
	{
		utils::create_textures(multisample, &m_depth_attachment, 1);
		utils::bind_texture(multisample, m_depth_attachment);
		switch (m_depth_attachment_specification.texture_format)
		{
		case framebuffer_texture_format::DEPTH24STENCIL8:
			utils::attach_depth_texture(m_depth_attachment, m_spec.samples, GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL_ATTACHMENT, m_spec.width, m_spec.height);
			break;
		}
	}

	if (m_color_attachments.size() > 1)
	{
		WHP_CORE_ASSERT(m_color_attachments.size() <= 4, "");
		GLenum buffers[4] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3 };
		glDrawBuffers(static_cast<GLsizei>(m_color_attachments.size()), buffers);
	}
	else if (m_color_attachments.empty())
	{
		// Only depth-pass
		glDrawBuffer(GL_NONE);
	}

	WHP_CORE_ASSERT(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "Framebuffer is incomplate");
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void opengl_framebuffer::flush()
{
	glDeleteFramebuffers(1, &m_renderer_id);
	glDeleteTextures(static_cast<GLsizei>(m_color_attachments.size()), m_color_attachments.data());
	glDeleteTextures(1, &m_depth_attachment);
}

void opengl_framebuffer::bind()
{
	glBindFramebuffer(GL_FRAMEBUFFER, m_renderer_id);
	glViewport(0, 0, m_spec.width, m_spec.height);
}

void opengl_framebuffer::unbind()
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void opengl_framebuffer::resize(uint32_t width, uint32_t height)
{
	if (width == 0 || height == 0 || width > s_max_framebuffer_size || height > s_max_framebuffer_size)
	{
		WHP_CORE_WARN("Attempted to resize framebuffer to {0}, {1}", width, height);
		return;
	}
	m_spec.width = width;
	m_spec.height = height;

	invalidate();
}

int opengl_framebuffer::read_pixel(uint32_t attachment_index, int x, int y)
{
	WHP_PROFILE_FUNCTION();
	WHP_CORE_ASSERT(attachment_index < m_color_attachments.size(), "attachment_index is out of the range!");

	glReadBuffer(GL_COLOR_ATTACHMENT0 + attachment_index);
	int pixel_data;
	glReadPixels(x, y, 1, 1, GL_RED_INTEGER, GL_INT, &pixel_data);
	return pixel_data;
}

void opengl_framebuffer::clear_attachment(uint32_t attachment_index, int value)
{
	WHP_CORE_ASSERT(attachment_index < m_color_attachments.size(), "attachment_index is out of the range!");

	auto& spec = m_color_attachment_specifications[attachment_index];
	glClearTexImage(m_color_attachments[attachment_index], 0,
		utils::whip_fb_texture_format_to_gl(spec.texture_format), GL_INT, &value);
}

_WHIP_END
