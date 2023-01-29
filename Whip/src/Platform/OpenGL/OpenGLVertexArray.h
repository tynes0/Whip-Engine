#pragma once

#include <Whip/Render/VertexArray.h>

_WHIP_START

class OpenGLVertexArray : public VertexArray
{
private:
	renderer_id_t m_RendererID = 0;
	std::vector<ref<VertexBuffer>> m_VertexBuffers;
	ref<IndexBuffer> m_IndexBuffer;
public:
	OpenGLVertexArray();
	virtual ~OpenGLVertexArray();

	virtual void Bind() const override;
	virtual void Unbind() const override;

	virtual void AddVertexBuffer(const ref<VertexBuffer>& vertexBuffer) override;
	virtual void SetIndexBuffer(const ref<IndexBuffer>& indexBuffer) override;

	WHP_NODISCARD virtual const std::vector<ref<VertexBuffer>>& GetVertexBuffers() const override { return m_VertexBuffers; }
	WHP_NODISCARD virtual const ref<IndexBuffer>& GetIndexBuffer() const override { return m_IndexBuffer; }
};

_WHIP_END