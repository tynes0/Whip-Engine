#include "whippch.h"
#include "Renderer2D.h"

#include <Whip/Render/VertexArray.h>
#include <Whip/Render/Shader.h>
#include <Whip/Render/RenderCommand.h>

#include <glm/gtc/matrix_transform.hpp>

_WHIP_START


struct quad_vertex
{
	glm::vec3 position;
	glm::vec4 color;
	glm::vec2 texture_coord;
	// TODO : texture id
};

struct renderer2D_data
{
	const uint32_t max_quads = 10000;
	const uint32_t max_vertices = max_quads * 4;
	const uint32_t max_indices = max_quads * 6;

	ref<vertex_array> quad_vertex_arr;
	ref<vertex_buffer> quad_vertex_buffer;
	ref<shader> texture_shader;
	ref<texture2D> white_texture;

	uint32_t quad_index_count = 0;

	quad_vertex* quad_vertex_buffer_base	= nullptr;
	quad_vertex* quad_vertex_buffer_ptr		= nullptr;
};

static renderer2D_data s_data;

void renderer2D::init()
{
	WHP_PROFILE_FUNCTION();

	s_data.quad_vertex_arr = vertex_array::create();
	s_data.quad_vertex_buffer;
	s_data.quad_vertex_buffer = vertex_buffer::create(s_data.max_vertices * sizeof(quad_vertex));
	s_data.quad_vertex_buffer->set_layout({
			{whip::shader_data_type::Float3, "a_position"},
			{whip::shader_data_type::Float4, "a_color"},
			{whip::shader_data_type::Float2, "a_texture_coord"}
		});
	s_data.quad_vertex_arr->add_vertex_buffer(s_data.quad_vertex_buffer);

	s_data.quad_vertex_buffer_base = new quad_vertex[s_data.max_vertices];

	uint32_t* quad_indices = new uint32_t[s_data.max_indices];

	uint32_t offset = 0;

#pragma warning (push)
#pragma warning (disable : 6386)

	for (uint32_t i = 0; i < s_data.max_indices; i += 6)
	{
		quad_indices[i + 0] = offset + 0;
		quad_indices[i + 1] = offset + 1;
		quad_indices[i + 2] = offset + 2;
		quad_indices[i + 3] = offset + 2;
		quad_indices[i + 4] = offset + 3;
		quad_indices[i + 5] = offset + 0;

		offset += 4;
	}

#pragma warning(pop)

	ref<index_buffer> quad_index_buffer;
	quad_index_buffer = index_buffer::create(quad_indices, s_data.max_indices);
	s_data.quad_vertex_arr->set_index_buffer(quad_index_buffer);
	delete[] quad_indices;

	s_data.white_texture = texture2D::create(1, 1);
	uint32_t white_texture_data = 0xffffffff;
	s_data.white_texture->set_data(&white_texture_data, sizeof(white_texture_data));

	s_data.texture_shader = shader::create("assets\\shaders\\texture.glsl");

	s_data.texture_shader->bind();
	s_data.texture_shader->set_int("u_texture", 0);
}

void renderer2D::shutdown()
{
	WHP_PROFILE_FUNCTION();
}

void renderer2D::begin_scene(const orthographic_camera& camera)
{
	WHP_PROFILE_FUNCTION();

	s_data.texture_shader->bind();
	s_data.texture_shader->set_mat4("u_view_projection", camera.get_view_projection_matrix());

	s_data.quad_index_count = 0;
	s_data.quad_vertex_buffer_ptr = s_data.quad_vertex_buffer_base;
}

void renderer2D::end_scene()
{
	WHP_PROFILE_FUNCTION();

	uint32_t data_size = (uint32_t)((uint8_t*)s_data.quad_vertex_buffer_ptr - (uint8_t*)s_data.quad_vertex_buffer_base);
	s_data.quad_vertex_buffer->set_data(s_data.quad_vertex_buffer_base, data_size);

	flush();
}

void renderer2D::flush()
{
	render_command::draw_indexed(s_data.quad_vertex_arr, s_data.quad_index_count);
}

void renderer2D::draw_quad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color)
{
	draw_quad({ position.x, position.y, 0.0f }, size, color);
}

void renderer2D::draw_quad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color)
{
	WHP_PROFILE_FUNCTION();

	s_data.quad_vertex_buffer_ptr->position = position;
	s_data.quad_vertex_buffer_ptr->color = color;
	s_data.quad_vertex_buffer_ptr->texture_coord = { 0.0f, 0.0f };
	s_data.quad_vertex_buffer_ptr++;

	s_data.quad_vertex_buffer_ptr->position = { position.x + size.x, position.y, 0.0f };
	s_data.quad_vertex_buffer_ptr->color = color;
	s_data.quad_vertex_buffer_ptr->texture_coord = { 0.0f, 0.0f };
	s_data.quad_vertex_buffer_ptr++;

	s_data.quad_vertex_buffer_ptr->position = { position.x + size.x, position.y + size.y, 0.0f };
	s_data.quad_vertex_buffer_ptr->color = color;
	s_data.quad_vertex_buffer_ptr->texture_coord = { 0.0f, 0.0f };
	s_data.quad_vertex_buffer_ptr++;

	s_data.quad_vertex_buffer_ptr->position = { position.x, position.y + size.y, 0.0f };
	s_data.quad_vertex_buffer_ptr->color = color;
	s_data.quad_vertex_buffer_ptr->texture_coord = { 0.0f, 0.0f };
	s_data.quad_vertex_buffer_ptr++;

	s_data.quad_index_count += 6;

	/*s_data.texture_shader->set_float("u_tiling_factor", 1.0f);
	s_data.white_texture->bind();

	glm::mat4 transform = glm::translate(glm::mat4(1.0f), position) * glm::scale(glm::mat4(1.0f), {size.x, size.y, 1.0f} );
	s_data.texture_shader->set_mat4("u_transform", transform);

	s_data.quad_vertex_arr->bind();
	render_command::draw_indexed(s_data.quad_vertex_arr);*/
}

void renderer2D::draw_quad(const glm::vec2& position, const glm::vec2& size, const ref<texture2D>& texture, float tiling_factor, const glm::vec4& tint_color)
{
	draw_quad({ position.x, position.y, 0.0f }, size, texture, tiling_factor, tint_color);
}

void renderer2D::draw_quad(const glm::vec3& position, const glm::vec2& size, const ref<texture2D>& texture, float tiling_factor, const glm::vec4& tint_color)
{
	WHP_PROFILE_FUNCTION();

	s_data.texture_shader->set_float4("u_color", tint_color);
	s_data.texture_shader->set_float("u_tiling_factor", tiling_factor);
	texture->bind();
	
	glm::mat4 transform = glm::translate(glm::mat4(1.0f), position) * glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });
	s_data.texture_shader->set_mat4("u_transform", transform);

	s_data.quad_vertex_arr->bind();
	render_command::draw_indexed(s_data.quad_vertex_arr);
}

void renderer2D::draw_rotated_quad(const glm::vec2& position, const glm::vec2& size, float rotation, const glm::vec4& color)
{
	draw_rotated_quad({ position.x, position.y, 0.0f }, size, rotation, color);
}

void renderer2D::draw_rotated_quad(const glm::vec3& position, const glm::vec2& size, float rotation, const glm::vec4& color)
{
	WHP_PROFILE_FUNCTION();

	s_data.texture_shader->set_float4("u_color", color);
	s_data.texture_shader->set_float("u_tiling_factor", 1.0f);
	s_data.white_texture->bind();

	glm::mat4 transform = glm::translate(glm::mat4(1.0f), position) * glm::rotate(glm::mat4(1.0f), rotation, { 0.0f, 0.0f, 1.0f }) * glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });
	s_data.texture_shader->set_mat4("u_transform", transform);

	s_data.quad_vertex_arr->bind();
	render_command::draw_indexed(s_data.quad_vertex_arr);
}

void renderer2D::draw_rotated_quad(const glm::vec2& position, const glm::vec2& size, float rotation, const ref<texture2D>& texture, float tiling_factor, const glm::vec4& tint_color)
{
	draw_rotated_quad({ position.x, position.y, 0.0f }, size, rotation, texture, tiling_factor, tint_color);
}

void renderer2D::draw_rotated_quad(const glm::vec3& position, const glm::vec2& size, float rotation, const ref<texture2D>& texture, float tiling_factor, const glm::vec4& tint_color)
{
	WHP_PROFILE_FUNCTION();

	s_data.texture_shader->set_float4("u_color", tint_color);
	s_data.texture_shader->set_float("u_tiling_factor", tiling_factor);
	texture->bind();

	glm::mat4 transform = glm::translate(glm::mat4(1.0f), position) * glm::rotate(glm::mat4(1.0f), rotation, { 0.0f, 0.0f, 1.0f }) * glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });
	s_data.texture_shader->set_mat4("u_transform", transform);

	s_data.quad_vertex_arr->bind();
	render_command::draw_indexed(s_data.quad_vertex_arr);
}

_WHIP_END