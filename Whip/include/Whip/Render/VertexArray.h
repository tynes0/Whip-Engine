#pragma once

#include <Whip/Render/Buffer.h>
#include <Whip/Core/Memory.h>
#include <vector>

_WHIP_START

class VertexArray
{
public:
	virtual ~VertexArray() = default;

	virtual void Bind() const = 0;
	virtual void Unbind() const = 0;

	virtual void AddVertexBuffer(const whip::Ref<VertexBuffer>& vertexBuffer) = 0;
	virtual void SetIndexBuffer(const whip::Ref<IndexBuffer>& indexBuffer) = 0;

	virtual const std::vector<whip::Ref<VertexBuffer>>& GetVertexBuffer() const = 0;
	virtual const whip::Ref<IndexBuffer>& GetIndexBuffer() const = 0;

	WHP_NODISCARD static Ref<VertexArray> Create();
};

_WHIP_END
