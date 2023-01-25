#pragma once

#include <Whip/Render/VertexArray.h>

_WHIP_START

class OpenGLVertexArray : public VertexArray
{
private:
	renderer_id_t m_RendererID = 0;
	std::vector<std::shared_ptr<VertexBuffer>> m_VertexBuffers;
	std::shared_ptr<IndexBuffer> m_IndexBuffer;
public:
	OpenGLVertexArray();
	virtual ~OpenGLVertexArray();

	virtual void Bind() const override;
	virtual void Unbind() const override;

	virtual void AddVertexBuffer(const std::shared_ptr<VertexBuffer>& vertexBuffer) override;
	virtual void SetIndexBuffer(const std::shared_ptr<IndexBuffer>& indexBuffer) override;

	WHP_NODISCARD virtual const std::vector<std::shared_ptr<VertexBuffer>>& GetVertexBuffers() const override { return m_VertexBuffers; }
	WHP_NODISCARD virtual const std::shared_ptr<IndexBuffer>& GetIndexBuffer() const override { return m_IndexBuffer; }
};

_WHIP_END