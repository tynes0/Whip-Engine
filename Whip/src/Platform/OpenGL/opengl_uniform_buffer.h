#pragma once

#include <Whip/Render/uniform_buffer.h>

_WHIP_START

class opengl_uniform_buffer : public uniform_buffer
{
public:
	opengl_uniform_buffer(uint32_t size, uint32_t binding);
	virtual ~opengl_uniform_buffer();

	virtual void set_data(const void* data, uint32_t size, uint32_t offset = 0) override;
private:
	renderer_id_t m_rendererID = 0;
};

_WHIP_END
