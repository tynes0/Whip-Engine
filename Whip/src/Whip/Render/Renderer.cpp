#include "whippch.h"
#include "Renderer.h"

_WHIP_START

void Renderer::BeginScene()
{
}

void Renderer::EndScene()
{
}

void Renderer::Submit(const std::shared_ptr<VertexArray>& vertexArray)
{
	vertexArray->Bind();
	RenderCommand::DrawIndexed(vertexArray);
}

_WHIP_END
