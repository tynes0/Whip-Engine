#include "whippch.h"
#include "Renderer.h"
#include <Platform/OpenGL/OpenGLShader.h>
#include <Whip/Render/Renderer2D.h>

_WHIP_START

ref<scene_data> renderer::m_scene_data = make_ref<scene_data>();

void renderer::init()
{
	WHP_PROFILE_FUNCTION();

	render_command::init();
	renderer2D::init();
}

void renderer::shutdown()
{
	WHP_PROFILE_FUNCTION();

	renderer2D::shutdown();
}

void renderer::on_window_resize(uint32_t width, uint32_t height)
{
	render_command::set_viewport(0, 0, width, height);
}

void renderer::begin_scene(orthographic_camera& camera)
{
	m_scene_data->view_projection_matrix = camera.get_view_projection_matrix();
}

void renderer::end_scene()
{
}

void renderer::submit(const ref<shader>& shader, const ref<vertex_array>& vertexArray, const glm::mat4& transform)
{
	shader->bind();
	dynamic_pointer_cast<opengl_shader>(shader)->upload_uniform_mat4("u_view_projection", m_scene_data->view_projection_matrix);
	dynamic_pointer_cast<opengl_shader>(shader)->upload_uniform_mat4("u_transform", transform);
	
	vertexArray->bind();
	render_command::draw_indexed(vertexArray);
}

_WHIP_END
