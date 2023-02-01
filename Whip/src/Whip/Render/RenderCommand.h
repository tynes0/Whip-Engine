#pragma once

#include "RenderAPI.h"

_WHIP_START

class RenderCommand
{
private:
	static RenderAPI* s_RenderAPI;
public:
	inline static void init()
	{
		s_RenderAPI->init();
	}

	inline static void SetClearColor(const glm::vec4& color)
	{
		s_RenderAPI->SetClearColor(color);
	}
	inline static void Clear()
	{
		s_RenderAPI->Clear();
	}
	inline static void DrawIndexed(const Whip::ref<VertexArray>& vertexArray)
	{
		s_RenderAPI->DrawIndexed(vertexArray);
	}
};

_WHIP_END