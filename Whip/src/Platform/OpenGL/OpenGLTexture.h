#pragma once

#include <Whip/Render/Texture.h>

#include <glad/glad.h>

_WHIP_START

class OpenGLTexture2D : public Texture2D
{
public:
	OpenGLTexture2D(const TextureSpecification& specification, RawBuffer data = RawBuffer());
	virtual ~OpenGLTexture2D();

	virtual const TextureSpecification& GetSpecification() const override { return m_Specification; }

	WHP_NODISCARD virtual uint32_t GetWidth() const override { return m_Specification.m_Width; }
	WHP_NODISCARD virtual uint32_t GetHeight() const override { return m_Specification.m_Height; }

	WHP_NODISCARD virtual RendererId GetRendererId() const override { return m_RendererID; }

	virtual void SetData(RawBuffer data) override;
	virtual RawBuffer GetData() const override;
	virtual void Bind(uint32_t slot = 0) const override;
	virtual bool IsLoaded() const override { return m_IsLoaded; }
	virtual bool operator==(const Texture& other) const override;
private:
	TextureSpecification m_Specification;

	bool m_IsLoaded = false;
	RendererId m_RendererID = 0;
	GLenum m_InternalFormat, m_DataFormat;
};

_WHIP_END
