#include "WhipPch.h"
#include <Whip/Render/Renderer.h>
#include <Platform/OpenGL/OpenGLShader.h>
#include <Whip/Render/Renderer2D.h>

_WHIP_START

Ref<SceneData> Renderer::s_SceneData = MakeRef<SceneData>();

void Renderer::Init()
{
	WHP_PROFILE_FUNCTION();

	RenderCommand::Init();
	Renderer2D::Init();
}

void Renderer::Shutdown()
{
	WHP_PROFILE_FUNCTION();

	Renderer2D::Shutdown();
	RenderCommand::Shutdown();
}

void Renderer::OnWindowResize(uint32_t width, uint32_t height)
{
	RenderCommand::SetViewport(0, 0, width, height);
}

void Renderer::BeginScene(OrthographicCamera& camera)
{
	s_SceneData->m_ViewProjectionMatrix = camera.GetViewProjectionMatrix();
}

void Renderer::EndScene()
{
}

void Renderer::Submit(const Ref<Shader>& shader, const Ref<VertexArray>& vertexArray, const glm::mat4& transform)
{
	shader->Bind();
	dynamic_pointer_cast<OpenGLShader>(shader)->UploadUniformMat4("u_view_projection", s_SceneData->m_ViewProjectionMatrix);
	dynamic_pointer_cast<OpenGLShader>(shader)->UploadUniformMat4("u_transform", transform);
	
	vertexArray->Bind();
	RenderCommand::DrawIndexed(vertexArray);
}

_WHIP_END
