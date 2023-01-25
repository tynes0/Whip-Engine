#pragma once

#include "RenderAPI.h"

_WHIP_START

class RenderCommand
{
private:
	static RenderAPI* s_RenderAPI;
public:
	inline static void SetClearColor(const glm::vec4& color)
	{
		s_RenderAPI->SetClearColor(color);
	}
	inline static void Clear()
	{
		s_RenderAPI->Clear();
	}
	inline static void DrawIndexed(const std::shared_ptr<VertexArray>& vertexArray)
	{
		s_RenderAPI->DrawIndexed(vertexArray);
	}
};

_WHIP_END