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
	float texture_index;
	float tiling_factor;
	// TODO : texture id
};

struct renderer2D_data
{
	static const uint32_t max_quads = 20000;
	static const uint32_t max_vertices = max_quads * 4;
	static const uint32_t max_indices = max_quads * 6;
	static const uint32_t max_texture_slots = 32; // TODO: render_caps

	ref<vertex_array> quad_vertex_arr;
	ref<vertex_buffer> quad_vertex_buffer;
	ref<shader> texture_shader;
	ref<texture2D> white_texture;

	uint32_t quad_index_count = 0;

	quad_vertex* quad_vertex_buffer_base = nullptr;
	quad_vertex* quad_vertex_buffer_ptr = nullptr;

	std::array <ref<texture2D>, max_texture_slots> texture_slots;
	uint32_t texture_slot_index = 1; // 0 = white texture

	glm::vec4 quad_vertex_positions[4] = {};

	renderer2D::statistics stats;
};

static renderer2D_data s_data;

inline void set_and_increment_quad_vertex_buffer_ptr(const glm::vec3& position, const glm::vec4& color, glm::vec2 texture_coord, float texture_index, float tiling_factor)
{
	s_data.quad_vertex_buffer_ptr->position = position;
	s_data.quad_vertex_buffer_ptr->color = color;
	s_data.quad_vertex_buffer_ptr->texture_coord = texture_coord;
	s_data.quad_vertex_buffer_ptr->texture_index = texture_index;
	s_data.quad_vertex_buffer_ptr->tiling_factor = tiling_factor;
	s_data.quad_vertex_buffer_ptr++;
}

void renderer2D::init()
{
	WHP_PROFILE_FUNCTION();

	s_data.quad_vertex_arr = vertex_array::create();
	s_data.quad_vertex_buffer;
	s_data.quad_vertex_buffer = vertex_buffer::create(s_data.max_vertices * sizeof(quad_vertex));
	s_data.quad_vertex_buffer->set_layout({
			{whip::shader_data_type::Float3, "a_position"},
			{whip::shader_data_type::Float4, "a_color"},
			{whip::shader_data_type::Float2, "a_texture_coord"},
			{whip::shader_data_type::Float,  "a_texture_index"},
			{whip::shader_data_type::Float,  "a_tiling_factor"}
		});
	s_data.quad_vertex_arr->add_vertex_buffer(s_data.quad_vertex_buffer);

	s_data.quad_vertex_buffer_base = new quad_vertex[s_data.max_vertices];

	uint32_t* quad_indices = new uint32_t[s_data.max_indices];

	uint32_t offset = 0;

_WHP_PRAGMA_WARNING(push)
_WHP_PRAGMA_WARNING_DISABLE(6386)

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

_WHP_PRAGMA_WARNING(pop)

	ref<index_buffer> quad_index_buffer;
	quad_index_buffer = index_buffer::create(quad_indices, s_data.max_indices);
	s_data.quad_vertex_arr->set_index_buffer(quad_index_buffer);
	delete[] quad_indices;

	s_data.white_texture = texture2D::create(1, 1);
	uint32_t white_texture_data = 0xffffffff;
	s_data.white_texture->set_data(&white_texture_data, sizeof(white_texture_data));

	std::array<int, s_data.max_texture_slots> samplers;

	for (int i = 0; i < s_data.max_texture_slots; ++i)
		samplers[i] = i;


	s_data.texture_shader = shader::create("texture", "assets\\shaders\\texture.vert", "assets\\shaders\\texture.frag");

	s_data.texture_shader->bind();
	s_data.texture_shader->set_int_array("u_textures", samplers.data(), s_data.max_texture_slots);

	s_data.texture_slots[0] = s_data.white_texture;

	s_data.quad_vertex_positions[0] = { -0.5f, -0.5f, 0.0f, 1.0f };
	s_data.quad_vertex_positions[1] = {  0.5f, -0.5f, 0.0f, 1.0f };
	s_data.quad_vertex_positions[2] = {  0.5f,	0.5f, 0.0f, 1.0f };
	s_data.quad_vertex_positions[3] = { -0.5f,  0.5f, 0.0f, 1.0f };
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

	s_data.texture_slot_index = 1;
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
	// bind textures
	for (uint32_t i = 0; i < s_data.texture_slot_index; ++i)
		s_data.texture_slots[i]->bind(i);

	render_command::draw_indexed(s_data.quad_vertex_arr, s_data.quad_index_count);
	s_data.stats.draw_calls++;
}

void renderer2D::draw_quad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color)
{
	draw_quad({ position.x, position.y, 0.0f }, size, color);
}

void renderer2D::draw_quad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color)
{
	WHP_PROFILE_FUNCTION();

	if (s_data.quad_index_count >= renderer2D_data::max_indices)
		flush_and_reset();

	constexpr size_t quad_vertex_count = 4u;
	constexpr float texture_index = 0.0f; // white color
	constexpr float tiling_factor = 1.0f;
	constexpr glm::vec2 texture_coords[] = { {0.0f,0.0f},{1.0f,0.0f},{1.0f,1.0f},{0.0f,1.0f} };

	glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
		* glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

	for (size_t i = 0; i < quad_vertex_count; ++i)
		set_and_increment_quad_vertex_buffer_ptr(transform * s_data.quad_vertex_positions[i], color, texture_coords[i], texture_index, tiling_factor);
	s_data.quad_index_count += 6;
	s_data.stats.quad_counts++;
}

void renderer2D::draw_quad(const glm::vec2& position, const glm::vec2& size, const ref<texture2D>& tex, float tiling_factor, const glm::vec4& tint_color)
{
	draw_quad({ position.x, position.y, 0.0f }, size, tex, tiling_factor, tint_color);
}

void renderer2D::draw_quad(const glm::vec3& position, const glm::vec2& size, const ref<texture2D>& tex, float tiling_factor, const glm::vec4& tint_color)
{
	WHP_PROFILE_FUNCTION();

	if (s_data.quad_index_count >= renderer2D_data::max_indices)
		flush_and_reset();

	constexpr size_t quad_vertex_count = 4u;
	constexpr glm::vec4 color = { 1.0f,1.0f,1.0f,1.0f };
	constexpr glm::vec2 texture_coords[] = { {0.0f,0.0f},{1.0f,0.0f},{1.0f,1.0f},{0.0f,1.0f} };

	float texture_index = 0.0f;

	for (uint32_t i = 1; i < s_data.texture_slot_index; ++i)
		if (DREF(s_data.texture_slots[i].get()) == DREF(tex.get()))
			texture_index = (float)i;

	if (texture_index == 0.0f)
	{
		texture_index = (float)s_data.texture_slot_index;
		s_data.texture_slots[s_data.texture_slot_index++] = tex;
	}

	glm::mat4 transform = glm::translate(glm::mat4(1.0f), position) * glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

	for (size_t i = 0; i < quad_vertex_count; ++i)
		set_and_increment_quad_vertex_buffer_ptr(transform * s_data.quad_vertex_positions[i], color, texture_coords[i], texture_index, tiling_factor);

	s_data.quad_index_count += 6;
	s_data.stats.quad_counts++;
}

void renderer2D::draw_quad(const glm::vec2& position, const glm::vec2& size, const ref<sub_texture2D>& sub_tex, float tiling_factor, const glm::vec4& tint_color)
{
	draw_quad({ position.x, position.y, 0.0f }, size, sub_tex, tiling_factor, tint_color);
}

void renderer2D::draw_quad(const glm::vec3& position, const glm::vec2& size, const ref<sub_texture2D>& sub_tex, float tiling_factor, const glm::vec4& tint_color)
{
	WHP_PROFILE_FUNCTION();

	if (s_data.quad_index_count >= renderer2D_data::max_indices)
		flush_and_reset();

	constexpr size_t quad_vertex_count = 4u;
	constexpr glm::vec4 color = { 1.0f,1.0f,1.0f,1.0f };
	const glm::vec2* texture_coords = sub_tex->get_texture_coords();
	auto tex = sub_tex->get_texture();

	float texture_index = 0.0f;

	for (uint32_t i = 1; i < s_data.texture_slot_index; ++i)
		if (DREF(s_data.texture_slots[i].get()) == DREF(tex.get()))
			texture_index = (float)i;

	if (texture_index == 0.0f)
	{
		texture_index = (float)s_data.texture_slot_index;
		s_data.texture_slots[s_data.texture_slot_index++] = tex;
	}

	glm::mat4 transform = glm::translate(glm::mat4(1.0f), position) * glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

	for (size_t i = 0; i < quad_vertex_count; ++i)
		set_and_increment_quad_vertex_buffer_ptr(transform * s_data.quad_vertex_positions[i], color, texture_coords[i], texture_index, tiling_factor);

	s_data.quad_index_count += 6;
	s_data.stats.quad_counts++;
}

void renderer2D::draw_rotated_quad(const glm::vec2& position, const glm::vec2& size, float rotation, const glm::vec4& color)
{
	draw_rotated_quad({ position.x, position.y, 0.0f }, size, rotation, color);
}

void renderer2D::draw_rotated_quad(const glm::vec3& position, const glm::vec2& size, float rotation, const glm::vec4& color)
{
	WHP_PROFILE_FUNCTION();

	if (s_data.quad_index_count >= renderer2D_data::max_indices)
		flush_and_reset();

	constexpr size_t quad_vertex_count = 4u;
	constexpr float texture_index = 0.0f; // white color
	constexpr float tiling_factor = 1.0f;
	constexpr glm::vec2 texture_coords[] = { {0.0f,0.0f},{1.0f,0.0f},{1.0f,1.0f},{0.0f,1.0f} };

	glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
		* glm::rotate(glm::mat4(1.0f), rotation, { 0.0f, 0.0f, 1.0f })
		* glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

	for (size_t i = 0; i < quad_vertex_count; ++i)
		set_and_increment_quad_vertex_buffer_ptr(transform * s_data.quad_vertex_positions[i], color, texture_coords[i], texture_index, tiling_factor);

	s_data.quad_index_count += 6;
	s_data.stats.quad_counts++;
}

void renderer2D::draw_rotated_quad(const glm::vec2& position, const glm::vec2& size, float rotation, const ref<texture2D>& tex, float tiling_factor, const glm::vec4& tint_color)
{
	draw_rotated_quad({ position.x, position.y, 0.0f }, size, rotation, tex, tiling_factor, tint_color);
}

void renderer2D::draw_rotated_quad(const glm::vec3& position, const glm::vec2& size, float rotation, const ref<texture2D>& tex, float tiling_factor, const glm::vec4& tint_color)
{
	WHP_PROFILE_FUNCTION();

	if (s_data.quad_index_count >= renderer2D_data::max_indices)
		flush_and_reset();

	constexpr size_t quad_vertex_count = 4u;
	constexpr glm::vec4 color = { 1.0f,1.0f,1.0f,1.0f };
	constexpr glm::vec2 texture_coords[] = { {0.0f,0.0f},{1.0f,0.0f},{1.0f,1.0f},{0.0f,1.0f} };

	float texture_index = 0.0f;

	for (uint32_t i = 1; i < s_data.texture_slot_index; ++i)
		if (DREF(s_data.texture_slots[i].get()) == DREF(tex.get()))
			texture_index = (float)i;

	if (texture_index == 0.0f)
	{
		texture_index = (float)s_data.texture_slot_index;
		s_data.texture_slots[s_data.texture_slot_index++] = tex;
	}

	glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
		* glm::rotate(glm::mat4(1.0f), rotation, { 0.0f, 0.0f, 1.0f })
		* glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

	for (size_t i = 0; i < quad_vertex_count; ++i)
		set_and_increment_quad_vertex_buffer_ptr(transform * s_data.quad_vertex_positions[i], color, texture_coords[i], texture_index, tiling_factor);

	s_data.quad_index_count += 6;
	s_data.stats.quad_counts++;
}

void renderer2D::draw_rotated_quad(const glm::vec2& position, const glm::vec2& size, float rotation, const ref<sub_texture2D>& sub_tex, float tiling_factor, const glm::vec4& tint_color)
{
	draw_rotated_quad({ position.x, position.y, 0.0f }, size, rotation, sub_tex, tiling_factor, tint_color);
}

void renderer2D::draw_rotated_quad(const glm::vec3& position, const glm::vec2& size, float rotation, const ref<sub_texture2D>& sub_tex, float tiling_factor, const glm::vec4& tint_color)
{
	WHP_PROFILE_FUNCTION();

	if (s_data.quad_index_count >= renderer2D_data::max_indices)
		flush_and_reset();

	constexpr size_t quad_vertex_count = 4u;
	constexpr glm::vec4 color = { 1.0f,1.0f,1.0f,1.0f };
	const glm::vec2* texture_coords = sub_tex->get_texture_coords();
	auto tex = sub_tex->get_texture();

	float texture_index = 0.0f;

	for (uint32_t i = 1; i < s_data.texture_slot_index; ++i)
		if (DREF(s_data.texture_slots[i].get()) == DREF(tex.get()))
			texture_index = (float)i;

	if (texture_index == 0.0f)
	{
		texture_index = (float)s_data.texture_slot_index;
		s_data.texture_slots[s_data.texture_slot_index++] = tex;
	}

	glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
		* glm::rotate(glm::mat4(1.0f), rotation, { 0.0f, 0.0f, 1.0f })
		* glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

	for (size_t i = 0; i < quad_vertex_count; ++i)
		set_and_increment_quad_vertex_buffer_ptr(transform * s_data.quad_vertex_positions[i], color, texture_coords[i], texture_index, tiling_factor);

	s_data.quad_index_count += 6;
	s_data.stats.quad_counts++;
}

void renderer2D::reset_stats()
{
	memset(&s_data.stats, 0, sizeof(renderer2D::statistics));
}

renderer2D::statistics renderer2D::get_stats()
{
	return s_data.stats;
}

void renderer2D::flush_and_reset()
{
	end_scene();

	s_data.quad_index_count = 0;
	s_data.quad_vertex_buffer_ptr = s_data.quad_vertex_buffer_base;

	s_data.texture_slot_index = 1;
}

_WHIP_END