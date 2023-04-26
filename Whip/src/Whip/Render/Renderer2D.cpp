#include "whippch.h"
#include "Renderer2D.h"

#include <Whip/Render/VertexArray.h>
#include <Whip/Render/Shader.h>
#include <Whip/Render/RenderCommand.h>

#include <glm/gtc/matrix_transform.hpp>

_WHIP_START

struct renderer2D_data
{
	ref<vertex_array> vertex_arr;
	ref<shader> texture_shader;
	ref<texture2D> white_texture;
};

static renderer2D_data* s_data;

void renderer2D::init()
{
	s_data = new renderer2D_data;
	float square_verticies[4 * 5] =
	{
		-0.5f, -0.5f, 0.0f, 0.0f, 0.0f,
		 0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
		 0.5f,  0.5f, 0.0f, 1.0f, 1.0f,
		-0.5f,  0.5f, 0.0f, 0.0f, 1.0f
	};
	uint32_t square_indicies[6] = { 0,1,2,2,3,0 };

	s_data->vertex_arr = vertex_array::create();
	ref<vertex_buffer> square_vertex_buffer;
	square_vertex_buffer = vertex_buffer::create(square_verticies, sizeof(square_verticies));
	square_vertex_buffer->set_layout({
			{whip::shader_data_type::Float3, "a_Position"},
			{whip::shader_data_type::Float2, "a_texture_coord"}
		});
	s_data->vertex_arr->add_vertex_buffer(square_vertex_buffer);
	ref<index_buffer> square_index_buffer;
	square_index_buffer = index_buffer::create(square_indicies, sizeof(square_indicies) / sizeof(uint32_t));
	s_data->vertex_arr->set_index_buffer(square_index_buffer);

	s_data->white_texture = texture2D::create(1, 1);
	uint32_t white_texture_data = 0xffffffff;
	s_data->white_texture->set_data(&white_texture_data, sizeof(white_texture_data));

	s_data->texture_shader = shader::create("assets\\shaders\\texture.glsl");

	s_data->texture_shader->bind();
	s_data->texture_shader->set_int("u_texture", 0);
}

void renderer2D::shutdown()
{
	delete s_data;
}

void renderer2D::begin_scene(const orthographic_camera& camera)
{
	s_data->texture_shader->bind();
	s_data->texture_shader->set_mat4("u_view_projection", camera.get_view_projection_matrix());
}

void renderer2D::end_scene()
{
}

void renderer2D::draw_quad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color)
{
	draw_quad({ position.x, position.y, 0.0f }, size, color);
}

void renderer2D::draw_quad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color)
{
	s_data->texture_shader->set_float4("u_color", color);
	s_data->white_texture->bind();

	glm::mat4 transform = glm::translate(glm::mat4(1.0f), position) * glm::scale(glm::mat4(1.0f), {size.x, size.y, 1.0f} );
	s_data->texture_shader->set_mat4("u_transform", transform);

	s_data->vertex_arr->bind();
	render_command::draw_indexed(s_data->vertex_arr);
}

void renderer2D::draw_quad(const glm::vec2& position, const glm::vec2& size, const ref<texture2D>& texture)
{
	draw_quad({ position.x, position.y, 0.0f }, size, texture);
}

void renderer2D::draw_quad(const glm::vec3& position, const glm::vec2& size, const ref<texture2D>& texture)
{
	s_data->texture_shader->set_float4("u_color", glm::vec4(1.0f));
	texture->bind();
	
	glm::mat4 transform = glm::translate(glm::mat4(1.0f), position) * glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });
	s_data->texture_shader->set_mat4("u_transform", transform);

	s_data->vertex_arr->bind();
	render_command::draw_indexed(s_data->vertex_arr);
}

_WHIP_END