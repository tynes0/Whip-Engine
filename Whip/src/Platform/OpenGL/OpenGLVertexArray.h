#pragma once

#include <Whip/Render/VertexArray.h>

_WHIP_START

class opengl_vertex_array : public vertex_array
{
private:
	renderer_id_t m_rendererID = 0;
	uint32_t m_vertex_buffer_index = 0;
	std::vector<ref<vertex_buffer>> m_vertex_buffers;
	ref<index_buffer> m_index_buffers;
public:
	opengl_vertex_array();
	virtual ~opengl_vertex_array();

	virtual void bind() const override;
	virtual void unbind() const override;

	virtual void add_vertex_buffer(const ref<vertex_buffer>& vertexBuffer) override;
	virtual void set_index_buffer(const ref<index_buffer>& indexBuffer) override;

	WHP_NODISCARD virtual const std::vector<ref<vertex_buffer>>& get_vertex_buffer() const override { return m_vertex_buffers; }
	WHP_NODISCARD virtual const ref<index_buffer>& get_index_buffer() const override { return m_index_buffers; }
};

_WHIP_END
