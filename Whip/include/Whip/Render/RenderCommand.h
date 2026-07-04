#pragma once

#include <Whip/Core/Memory.h>

#include "RenderAPI.h"

_WHIP_START

class RenderCommand
{
private:
	static RenderAPI* s_RenderAPI;
public:
	static void Init()
	{
		s_RenderAPI->Init();
	}

	static void Shutdown()
	{
		DeleteRaw(s_RenderAPI);
		s_RenderAPI = nullptr;
	}

	static void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
	{
		s_RenderAPI->SetViewport(x, y, width, height);
	}

	static void SetClearColor(const glm::vec4& color)
	{
		s_RenderAPI->SetClearColor(color);
	}

	static void Clear()
	{
		s_RenderAPI->Clear();
	}

	static void DrawIndexed(const whip::Ref<VertexArray>& vertexArray, uint32_t indexCount = 0)
	{
		s_RenderAPI->DrawIndexed(vertexArray, indexCount);
	}

	static void DrawLines(const whip::Ref<VertexArray>& vertexArray, uint32_t vertexCount = 0)
	{
		s_RenderAPI->DrawLines(vertexArray, vertexCount);
	}

	static void SetLineWidth(float width)
	{
		s_RenderAPI->SetLineWidth(width);
	}

};

_WHIP_END
