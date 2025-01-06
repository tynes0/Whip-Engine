#pragma once

#include <Whip/Render/Buffer.h>

_WHIP_START

class opengl_vertex_buffer : public vertex_buffer
{
private:
	renderer_id_t m_rendererID = 0;
	buffer_layout m_layout;
public:
	opengl_vertex_buffer(float* vertices, uint32_t size);
	opengl_vertex_buffer(uint32_t size);
	virtual ~opengl_vertex_buffer();

	virtual void bind() const override;
	virtual void unbind() const override;

	virtual void set_layout(const buffer_layout& layout) override { m_layout = layout; }
	WHP_NODISCARD virtual const buffer_layout& get_layout() const override { return m_layout; }

	virtual void set_data(const void* data, uint32_t size) override;
};

class opengl_index_buffer : public index_buffer
{
private:
	renderer_id_t m_rendererID;
	uint32_t m_count;
public:
	opengl_index_buffer(uint32_t* indices, uint32_t count);
	virtual ~opengl_index_buffer();

	WHP_NODISCARD virtual uint32_t get_count() const override { return m_count; };
	virtual void bind() const override;
	virtual void unbind() const override;
};

_WHIP_END
