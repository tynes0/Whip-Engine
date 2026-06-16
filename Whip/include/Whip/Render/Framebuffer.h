#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/Memory.h>

#include <vector>

_WHIP_START

enum class FramebufferTextureFormat
{
	None = 0,
	Rgba8,
	RedInteger,
	Depth24Stencil8,
	Depth = Depth24Stencil8
};

struct FramebufferTextureSpecification
{
	FramebufferTextureSpecification() = default;
	FramebufferTextureSpecification(FramebufferTextureFormat format) : m_TextureFormat(format) {}

	FramebufferTextureFormat m_TextureFormat = FramebufferTextureFormat::None;
};

struct FramebufferAttachmentSpecification
{
	FramebufferAttachmentSpecification() = default;
	FramebufferAttachmentSpecification(std::initializer_list<FramebufferTextureSpecification> attachments) : m_Attachments(attachments) {}

	std::vector<FramebufferTextureSpecification> m_Attachments;
};

struct FramebufferSpecification
{
	uint32_t m_Width = 0;
	uint32_t m_Height = 0;
	FramebufferAttachmentSpecification m_Attachments;
	uint32_t m_Samples = 1;

	bool m_SwapChainTarget = false;
};


class Framebuffer
{
public:
	virtual ~Framebuffer() = default;

	virtual void Bind() = 0;
	virtual void Unbind() = 0;
	virtual void Resize(uint32_t width, uint32_t height) = 0;
	virtual int ReadPixel(uint32_t attachmentIndex, int x, int y) = 0;
	virtual void ClearAttachment(uint32_t attachmentIndex, int value) = 0;
	virtual uint32_t GetColorAttachmentRendererId(uint32_t index = 0) const = 0;
	virtual const FramebufferSpecification& GetSpecification() const = 0;

	static Ref<Framebuffer> Create(const FramebufferSpecification& spec);
};

_WHIP_END
