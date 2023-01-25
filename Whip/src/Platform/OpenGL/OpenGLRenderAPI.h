#pragma once

#include <Whip/Render/RenderAPI.h>

_WHIP_START

class OpenGLRenderAPI : public RenderAPI
{
	virtual void SetClearColor(const glm::vec4& color) override;
	virtual void Clear() override;
	virtual void DrawIndexed(const std::shared_ptr<VertexArray>& vertexArray) override;
};

_WHIP_END