#pragma once

#include <Whip/Render/Buffer.h>
#include <memory>

_WHIP_START

class VertexArray
{
public:
	virtual ~VertexArray() {}

	virtual void Bind() const = 0;
	virtual void Unbind() const = 0;

	virtual void AddVertexBuffer(const Whip::ref<VertexBuffer>& vertexBuffer) = 0;
	virtual void SetIndexBuffer(const Whip::ref<IndexBuffer>& indexBuffer) = 0;

	virtual const std::vector<Whip::ref<VertexBuffer>>& GetVertexBuffers() const = 0;
	virtual const Whip::ref<IndexBuffer>& GetIndexBuffer() const = 0;

	WHP_NODISCARD static ref<VertexArray> Create();
};

_WHIP_END