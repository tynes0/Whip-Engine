#pragma once
#include <Whip/Render/Framebuffer.h>

_WHIP_START

class OpenGLFramebuffer : public Framebuffer
{
public:
	OpenGLFramebuffer(const FramebufferSpecification& spec);
	~OpenGLFramebuffer();

	void Invalidate();
	void Flush();

	virtual void Bind() override;
	virtual void Unbind() override;
	virtual void Resize(uint32_t width, uint32_t height) override;
	virtual int ReadPixel(uint32_t attachmentIndex, int x, int y) override;
	virtual void ClearAttachment(uint32_t attachmentIndex, int value) override;
	virtual const FramebufferSpecification& GetSpecification() const override { return m_Spec; }
	virtual uint32_t GetColorAttachmentRendererId(uint32_t index = 0) const override { WHP_CORE_ASSERT(index < m_ColorAttachments.size(), "color attachment index out of the range"); return m_ColorAttachments[index]; }
private:
	RendererId m_RendererId = 0;
	FramebufferSpecification m_Spec;
	std::vector<FramebufferTextureSpecification> m_ColorAttachmentSpecifications;
	FramebufferTextureSpecification m_DepthAttachmentSpecification = FramebufferTextureFormat::None;
	std::vector<uint32_t> m_ColorAttachments;
	uint32_t m_DepthAttachment = 0;
};

_WHIP_END
