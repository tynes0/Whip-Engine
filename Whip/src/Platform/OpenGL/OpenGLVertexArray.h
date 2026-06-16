#pragma once

#include <Whip/Render/VertexArray.h>

_WHIP_START

class OpenGLVertexArray : public VertexArray
{
private:
	RendererId m_RendererID = 0;
	uint32_t m_VertexBufferIndex = 0;
	std::vector<Ref<VertexBuffer>> m_VertexBuffers;
	Ref<IndexBuffer> m_IndexBuffer;
public:
	OpenGLVertexArray();
	virtual ~OpenGLVertexArray();

	virtual void Bind() const override;
	virtual void Unbind() const override;

	virtual void AddVertexBuffer(const Ref<VertexBuffer>& vertexBuffer) override;
	virtual void SetIndexBuffer(const Ref<IndexBuffer>& indexBuffer) override;

	WHP_NODISCARD virtual const std::vector<Ref<VertexBuffer>>& GetVertexBuffer() const override { return m_VertexBuffers; }
	WHP_NODISCARD virtual const Ref<IndexBuffer>& GetIndexBuffer() const override { return m_IndexBuffer; }
};

_WHIP_END
