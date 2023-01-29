#include "whippch.h"
#include "Renderer.h"
#include <Platform/OpenGL/OpenGLShader.h>

_WHIP_START

scene_data* Renderer::m_scene_data = new scene_data();

void Renderer::begin_scene(orthographic_camera& camera)
{
	m_scene_data->view_projection_matrix = camera.get_view_projection_matrix();
}

void Renderer::end_scene()
{
}

void Renderer::submit(const std::shared_ptr<Shader>& shader, const std::shared_ptr<VertexArray>& vertexArray, const glm::mat4& transform)
{
	shader->Bind();
	std::dynamic_pointer_cast<OpenGLShader>(shader)->upload_uniform_mat4("u_view_projection", m_scene_data->view_projection_matrix);
	std::dynamic_pointer_cast<OpenGLShader>(shader)->upload_uniform_mat4("u_transform", transform);
	
	vertexArray->Bind();
	RenderCommand::DrawIndexed(vertexArray);
}

_WHIP_END
