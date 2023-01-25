#pragma once

#include <Whip/Render/Buffer.h>

_WHIP_START

class OpenGLVertexBuffer : public VertexBuffer
{
private:
	renderer_id_t m_RendererID = 0;
	BufferLayout m_Layout;
public:
	OpenGLVertexBuffer(float* vertices, uint32_t size);
	virtual ~OpenGLVertexBuffer();

	virtual void Bind() const override;
	virtual void Unbind() const override;

	virtual void SetLayout(const BufferLayout& layout) override { m_Layout = layout; }
	WHP_NODISCARD virtual const BufferLayout& GetLayout() const override { return m_Layout; }
};

class OpenGLIndexBuffer : public IndexBuffer
{
private:
	renderer_id_t m_RendererID;
	uint32_t m_Count;
public:
	OpenGLIndexBuffer(uint32_t* indices, uint32_t count);
	virtual ~OpenGLIndexBuffer();

	WHP_NODISCARD virtual uint32_t GetCount() const override { return m_Count; };
	virtual void Bind() const override;
	virtual void Unbind() const override;
};

_WHIP_END
