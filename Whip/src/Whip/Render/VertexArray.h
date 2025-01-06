#pragma once

#include <Whip/Render/Buffer.h>
#include <Whip/Core/memory.h>
#include <vector>

_WHIP_START

class vertex_array
{
public:
	virtual ~vertex_array() = default;

	virtual void bind() const = 0;
	virtual void unbind() const = 0;

	virtual void add_vertex_buffer(const whip::ref<vertex_buffer>& vertexBuffer) = 0;
	virtual void set_index_buffer(const whip::ref<index_buffer>& indexBuffer) = 0;

	virtual const std::vector<whip::ref<vertex_buffer>>& get_vertex_buffer() const = 0;
	virtual const whip::ref<index_buffer>& get_index_buffer() const = 0;

	WHP_NODISCARD static ref<vertex_array> create();
};

_WHIP_END
