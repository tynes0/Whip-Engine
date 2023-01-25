#pragma once

#include "RenderCommand.h"

_WHIP_START

class Renderer
{
public:
	static void BeginScene();
	static void EndScene();
	static void Submit(const std::shared_ptr<VertexArray>& vertexArray);

	WHP_NODISCARD inline static RenderAPI::API GetAPI() { return RenderAPI::GetAPI(); }
};

_WHIP_END