#pragma once

#include <Whip/Render/UniformBuffer.h>

_WHIP_START

class OpenGLUniformBuffer : public UniformBuffer
{
public:
	OpenGLUniformBuffer(uint32_t size, uint32_t binding);
	virtual ~OpenGLUniformBuffer();

	virtual void SetData(const void* data, uint32_t size, uint32_t offset = 0) override;
private:
	RendererId m_RendererID = 0;
};

_WHIP_END
