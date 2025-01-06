#include "whippch.h"
#include "Renderer2D.h"

#include "VertexArray.h"
#include "Shader.h"
#include "RenderCommand.h"
#include "uniform_buffer.h"
#include "msdf_data.h"

#include <Whip/Asset/asset_manager.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <array>

_WHIP_START


struct quad_vertex
{
	glm::vec3 position;
	glm::vec4 color;
	glm::vec2 texture_coord;
	float texture_index;
	float tiling_factor;
	int entityID; // editor only
};

struct circle_vertex
{
	glm::vec3 world_position;
	glm::vec3 local_position;
	glm::vec4 color;
	float thickness;
	float fade;
	int entityID; // editor only
};

struct line_vertex
{
	glm::vec3 position;
	glm::vec4 color;
	int entityID; // editor only
};

struct text_vertex
{
	glm::vec3 position;
	glm::vec4 color;
	glm::vec2 texture_coord;
	// TODO: bg color for outline/bg
	int entityID; // editor only
};

struct renderer2D_data
{
	static const uint32_t max_quads = 20000;
	static const uint32_t max_vertices = max_quads * 4;
	static const uint32_t max_indices = max_quads * 6;
	static const uint32_t max_texture_slots = 32; // TODO: render_caps

	ref<vertex_array> quad_vertex_arr;
	ref<vertex_buffer> quad_vertex_buffer;
	ref<shader> quad_shader;
	ref<texture2D> white_texture;

	ref<vertex_array> circle_vertex_arr;
	ref<vertex_buffer> circle_vertex_buffer;
	ref<shader> circle_shader;

	ref<vertex_array> line_vertex_arr;
	ref<vertex_buffer> line_vertex_buffer;
	ref<shader> line_shader;

	ref<vertex_array> text_vertex_arr;
	ref<vertex_buffer> text_vertex_buffer;
	ref<shader> text_shader;

	uint32_t quad_index_count = 0;
	quad_vertex* quad_vertex_buffer_base = nullptr;
	quad_vertex* quad_vertex_buffer_ptr = nullptr;

	uint32_t circle_index_count = 0;
	circle_vertex* circle_vertex_buffer_base = nullptr;
	circle_vertex* circle_vertex_buffer_ptr = nullptr;

	uint32_t line_vertex_count = 0;
	line_vertex* line_vertex_buffer_base = nullptr;
	line_vertex* line_vertex_buffer_ptr = nullptr;

	uint32_t text_index_count = 0;
	text_vertex* text_vertex_buffer_base = nullptr;
	text_vertex* text_vertex_buffer_ptr = nullptr;

	std::array <ref<texture2D>, max_texture_slots> texture_slots;
	uint32_t texture_slot_index = 1; // 0 = white texture
	
	ref<texture2D> font_atlas_texture;

	glm::vec4 quad_vertex_positions[4] = {};

	renderer2D::statistics stats;

	struct camera_data
	{
		glm::mat4 view_projection;
	};
	camera_data camera_buffer{};
	ref<uniform_buffer> camera_uniform_buffer;
};

static renderer2D_data s_data;

static void set_and_increment_quad_vertex_buffer_ptr(const glm::vec3& position, const glm::vec4& color, glm::vec2 texture_coord, float texture_index, float tiling_factor, int entityID)
{
	s_data.quad_vertex_buffer_ptr->position = position;
	s_data.quad_vertex_buffer_ptr->color = color;
	s_data.quad_vertex_buffer_ptr->texture_coord = texture_coord;
	s_data.quad_vertex_buffer_ptr->texture_index = texture_index;
	s_data.quad_vertex_buffer_ptr->tiling_factor = tiling_factor;
	s_data.quad_vertex_buffer_ptr->entityID = entityID;
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
			{whip::shader_data_type::Float,  "a_tiling_factor"},
			{whip::shader_data_type::Int,  "a_entityID"}
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

	// Circles
	s_data.circle_vertex_arr = vertex_array::create();
	
	s_data.circle_vertex_buffer = vertex_buffer::create(s_data.max_vertices * sizeof(circle_vertex));
	s_data.circle_vertex_buffer->set_layout({
		{ shader_data_type::Float3, "a_world_position" },
		{ shader_data_type::Float3, "a_local_position" },
		{ shader_data_type::Float4, "a_color"         },
		{ shader_data_type::Float,  "a_thickness"     },
		{ shader_data_type::Float,  "a_fade"          },
		{ shader_data_type::Int,    "a_entityID"      }
		});
	s_data.circle_vertex_arr->add_vertex_buffer(s_data.circle_vertex_buffer);
	s_data.circle_vertex_arr->set_index_buffer(quad_index_buffer);
	s_data.circle_vertex_buffer_base = new circle_vertex[s_data.max_vertices];

	// lines
	s_data.line_vertex_arr = vertex_array::create();

	s_data.line_vertex_buffer = vertex_buffer::create(s_data.max_vertices * sizeof(line_vertex));
	s_data.line_vertex_buffer->set_layout({
		{ shader_data_type::Float3, "a_position" },
		{ shader_data_type::Float4, "a_color"    },
		{ shader_data_type::Int,    "a_entityID" }
		});
	s_data.line_vertex_arr->add_vertex_buffer(s_data.line_vertex_buffer);
	s_data.line_vertex_buffer_base = new line_vertex[s_data.max_vertices];

	// texts
	s_data.text_vertex_arr = vertex_array::create();

	s_data.text_vertex_buffer = vertex_buffer::create(s_data.max_vertices * sizeof(text_vertex));
	s_data.text_vertex_buffer->set_layout({
		{ shader_data_type::Float3, "a_position"     },
		{ shader_data_type::Float4, "a_color"        },
		{ shader_data_type::Float2, "a_texture_coord"},
		{ shader_data_type::Int,    "a_entityID"     }
		});
	s_data.text_vertex_arr->add_vertex_buffer(s_data.text_vertex_buffer);
	s_data.text_vertex_arr->set_index_buffer(quad_index_buffer);
	s_data.text_vertex_buffer_base = new text_vertex[s_data.max_vertices];

	s_data.white_texture = texture2D::create(texture_specification{});
	uint32_t white_texture_data = 0xffffffff;
	s_data.white_texture->set_data(raw_buffer(&white_texture_data, sizeof(white_texture_data)));

	std::array<int, s_data.max_texture_slots> samplers;

	for (int i = 0; i < s_data.max_texture_slots; ++i)
		samplers[i] = i;

	s_data.quad_shader		= shader::create("assets\\shaders\\renderer2D_quad.glsl");
	s_data.circle_shader	= shader::create("assets\\shaders\\renderer2D_circle.glsl");
	s_data.line_shader		= shader::create("assets\\shaders\\renderer2D_line.glsl");
	s_data.text_shader		= shader::create("assets\\shaders\\renderer2D_text.glsl");

	s_data.texture_slots[0] = s_data.white_texture;

	s_data.quad_vertex_positions[0] = { -0.5f, -0.5f, 0.0f, 1.0f };
	s_data.quad_vertex_positions[1] = {  0.5f, -0.5f, 0.0f, 1.0f };
	s_data.quad_vertex_positions[2] = {  0.5f,	0.5f, 0.0f, 1.0f };
	s_data.quad_vertex_positions[3] = { -0.5f,  0.5f, 0.0f, 1.0f };

	s_data.camera_uniform_buffer = uniform_buffer::create(sizeof(renderer2D_data::camera_data), 0);
}

void renderer2D::shutdown()
{
	delete[] s_data.quad_vertex_buffer_base;
	WHP_PROFILE_FUNCTION();
}

void renderer2D::begin_scene(const orthographic_camera& camera)
{
	WHP_PROFILE_FUNCTION();

	s_data.quad_shader->bind();
	s_data.quad_shader->set_mat4("u_view_projection", camera.get_view_projection_matrix());

	start_batch();
}

void renderer2D::begin_scene(const camera& cam, const glm::mat4& transform)
{
	WHP_PROFILE_FUNCTION();

	s_data.camera_buffer.view_projection = cam.get_projection() * glm::inverse(transform);
	s_data.camera_uniform_buffer->set_data(&s_data.camera_buffer, sizeof(renderer2D_data::camera_data));

	start_batch();
}

void renderer2D::begin_scene(const editor_camera& cam)
{
	WHP_PROFILE_FUNCTION();

	s_data.camera_buffer.view_projection = cam.get_view_projection();
	s_data.camera_uniform_buffer->set_data(&s_data.camera_buffer, sizeof(renderer2D_data::camera_data));

	start_batch();
}

void renderer2D::end_scene()
{
	WHP_PROFILE_FUNCTION();
	flush();
}

void renderer2D::flush()
{
	if (s_data.quad_index_count)
	{
		uint32_t dataSize = (uint32_t)((uint8_t*)s_data.quad_vertex_buffer_ptr - (uint8_t*)s_data.quad_vertex_buffer_base);
		s_data.quad_vertex_buffer->set_data(s_data.quad_vertex_buffer_base, dataSize);
		// Bind textures
		for (uint32_t i = 0; i < s_data.texture_slot_index; i++)
			s_data.texture_slots[i]->bind(i);
	
		s_data.quad_shader->bind();
		render_command::draw_indexed(s_data.quad_vertex_arr, s_data.quad_index_count);
		s_data.stats.draw_calls++;
	}

	if (s_data.circle_index_count)
	{
		uint32_t dataSize = (uint32_t)((uint8_t*)s_data.circle_vertex_buffer_ptr - (uint8_t*)s_data.circle_vertex_buffer_base);
		s_data.circle_vertex_buffer->set_data(s_data.circle_vertex_buffer_base, dataSize);

		s_data.circle_shader->bind();
		render_command::draw_indexed(s_data.circle_vertex_arr, s_data.circle_index_count);
		s_data.stats.draw_calls++;
	}

	if (s_data.line_vertex_count)
	{
		uint32_t dataSize = (uint32_t)((uint8_t*)s_data.line_vertex_buffer_ptr - (uint8_t*)s_data.line_vertex_buffer_base);
		s_data.line_vertex_buffer->set_data(s_data.line_vertex_buffer_base, dataSize);

		s_data.line_shader->bind();
		render_command::draw_lines(s_data.line_vertex_arr, s_data.line_vertex_count);
		s_data.stats.draw_calls++;
	}

	if (s_data.text_index_count)
	{
		uint32_t dataSize = (uint32_t)((uint8_t*)s_data.text_vertex_buffer_ptr - (uint8_t*)s_data.text_vertex_buffer_base);
		s_data.text_vertex_buffer->set_data(s_data.text_vertex_buffer_base, dataSize);

		auto buf = s_data.text_vertex_buffer_base;
		s_data.font_atlas_texture->bind(0);

		s_data.text_shader->bind();
		render_command::draw_indexed(s_data.text_vertex_arr, s_data.text_index_count);
		s_data.stats.draw_calls++;
	}
}


void renderer2D::draw_quad(const glm::mat4& transform, const glm::vec4& color, int entityID)
{
	WHP_PROFILE_FUNCTION();

	if (s_data.quad_index_count >= renderer2D_data::max_indices)
		next_batch();

	constexpr size_t quad_vertex_count = 4u;
	constexpr float texture_index = 0.0f; // white color
	constexpr float tiling_factor = 1.0f;
	constexpr glm::vec2 texture_coords[] = { {0.0f,0.0f},{1.0f,0.0f},{1.0f,1.0f},{0.0f,1.0f} };

	for (size_t i = 0; i < quad_vertex_count; ++i)
		set_and_increment_quad_vertex_buffer_ptr(transform * s_data.quad_vertex_positions[i], color, texture_coords[i], texture_index, tiling_factor, entityID);
	s_data.quad_index_count += 6;
	s_data.stats.quad_counts++;
}

void renderer2D::draw_quad(const glm::mat4& transform, const ref<texture2D>& tex, float tiling_factor, const glm::vec4& tint_color, int entityID)
{
	WHP_PROFILE_FUNCTION();
	WHP_CORE_VERIFY(tex);

	if (s_data.quad_index_count >= renderer2D_data::max_indices)
		next_batch();

	constexpr size_t quad_vertex_count = 4u;
	constexpr glm::vec2 texture_coords[] = { {0.0f,0.0f},{1.0f,0.0f},{1.0f,1.0f},{0.0f,1.0f} };

	float texture_index = 0.0f;

	for (uint32_t i = 1; i < s_data.texture_slot_index; ++i)
		if (DREF(s_data.texture_slots[i]) == DREF(tex))
		{
			texture_index = (float)i;
			break;
		}

_WHP_PRAGMA_WARNING(push)
_WHP_PRAGMA_WARNING_DISABLE(28020)
	if (texture_index == 0.0f)
	{
		if (s_data.texture_slot_index >= renderer2D_data::max_texture_slots)
			next_batch();
		texture_index = (float)s_data.texture_slot_index;
		s_data.texture_slots[s_data.texture_slot_index] = tex;
		s_data.texture_slot_index++;
	}
_WHP_PRAGMA_WARNING(pop)

	for (size_t i = 0; i < quad_vertex_count; ++i)
		set_and_increment_quad_vertex_buffer_ptr(transform * s_data.quad_vertex_positions[i], tint_color, texture_coords[i], texture_index, tiling_factor, entityID);

	s_data.quad_index_count += 6;
	s_data.stats.quad_counts++;
}

void renderer2D::draw_quad(const glm::mat4& transform, const ref<sub_texture2D>& sub_tex, float tiling_factor, const glm::vec4& tint_color, int entityID)
{
	WHP_PROFILE_FUNCTION();

	if (s_data.quad_index_count >= renderer2D_data::max_indices)
		next_batch();

	constexpr size_t quad_vertex_count = 4u;
	const glm::vec2* texture_coords = sub_tex->get_texture_coords();
	auto tex = sub_tex->get_texture();

	float texture_index = 0.0f;

	for (uint32_t i = 1; i < s_data.texture_slot_index; ++i)
		if (DREF(s_data.texture_slots[i].get()) == DREF(tex.get()))
			texture_index = (float)i;

	if (texture_index == 0.0f)
	{
		if (s_data.texture_slot_index >= renderer2D_data::max_texture_slots)
			next_batch();
		texture_index = (float)s_data.texture_slot_index;
		s_data.texture_slots[s_data.texture_slot_index++] = tex;
	}

	for (size_t i = 0; i < quad_vertex_count; ++i)
		set_and_increment_quad_vertex_buffer_ptr(transform * s_data.quad_vertex_positions[i], tint_color, texture_coords[i], texture_index, tiling_factor, entityID);

	s_data.quad_index_count += 6;
	s_data.stats.quad_counts++;
}


void renderer2D::draw_quad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color)
{
	draw_quad({ position.x, position.y, 0.0f }, size, color);
}

void renderer2D::draw_quad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color)
{
	glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
		* glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });
	draw_quad(transform, color);
}

void renderer2D::draw_quad(const glm::vec2& position, const glm::vec2& size, const ref<texture2D>& tex, float tiling_factor, const glm::vec4& tint_color)
{
	draw_quad({ position.x, position.y, 0.0f }, size, tex, tiling_factor, tint_color);
}

void renderer2D::draw_quad(const glm::vec3& position, const glm::vec2& size, const ref<texture2D>& tex, float tiling_factor, const glm::vec4& tint_color)
{
	glm::mat4 transform = glm::translate(glm::mat4(1.0f), position) * glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });
	draw_quad(transform, tex, tiling_factor, tint_color);
}

void renderer2D::draw_quad(const glm::vec2& position, const glm::vec2& size, const ref<sub_texture2D>& sub_tex, float tiling_factor, const glm::vec4& tint_color)
{
	draw_quad({ position.x, position.y, 0.0f }, size, sub_tex, tiling_factor, tint_color);
}

void renderer2D::draw_quad(const glm::vec3& position, const glm::vec2& size, const ref<sub_texture2D>& sub_tex, float tiling_factor, const glm::vec4& tint_color)
{
	glm::mat4 transform = glm::translate(glm::mat4(1.0f), position) * glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });
	draw_quad(transform, sub_tex, tiling_factor, tint_color);
}

void renderer2D::draw_rotated_quad(const glm::vec2& position, const glm::vec2& size, float rotation, const glm::vec4& color)
{
	draw_rotated_quad({ position.x, position.y, 0.0f }, size, rotation, color);
}

void renderer2D::draw_rotated_quad(const glm::vec3& position, const glm::vec2& size, float rotation, const glm::vec4& color)
{
	glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
		* glm::rotate(glm::mat4(1.0f), rotation, { 0.0f, 0.0f, 1.0f })
		* glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });
	draw_quad(transform, color);
}

void renderer2D::draw_rotated_quad(const glm::vec2& position, const glm::vec2& size, float rotation, const ref<texture2D>& tex, float tiling_factor, const glm::vec4& tint_color)
{
	draw_rotated_quad({ position.x, position.y, 0.0f }, size, rotation, tex, tiling_factor, tint_color);
}

void renderer2D::draw_rotated_quad(const glm::vec3& position, const glm::vec2& size, float rotation, const ref<texture2D>& tex, float tiling_factor, const glm::vec4& tint_color)
{
	glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
		* glm::rotate(glm::mat4(1.0f), rotation, { 0.0f, 0.0f, 1.0f })
		* glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

	draw_quad(transform, tex, tiling_factor, tint_color);
}

void renderer2D::draw_rotated_quad(const glm::vec2& position, const glm::vec2& size, float rotation, const ref<sub_texture2D>& sub_tex, float tiling_factor, const glm::vec4& tint_color)
{
	draw_rotated_quad({ position.x, position.y, 0.0f }, size, rotation, sub_tex, tiling_factor, tint_color);
}

void renderer2D::draw_rotated_quad(const glm::vec3& position, const glm::vec2& size, float rotation, const ref<sub_texture2D>& sub_tex, float tiling_factor, const glm::vec4& tint_color)
{
	glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
		* glm::rotate(glm::mat4(1.0f), rotation, { 0.0f, 0.0f, 1.0f })
		* glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

	draw_quad(transform, sub_tex, tiling_factor, tint_color);
}

void renderer2D::draw_circle(const glm::mat4& transform, const glm::vec4& color, float thickness, float fade, int entityID)
{
	WHP_PROFILE_FUNCTION();
	
	for (size_t i = 0; i < 4; i++)
	{
		s_data.circle_vertex_buffer_ptr->world_position = transform * s_data.quad_vertex_positions[i];
		s_data.circle_vertex_buffer_ptr->local_position = s_data.quad_vertex_positions[i] * 2.0f;
		s_data.circle_vertex_buffer_ptr->color = color;
		s_data.circle_vertex_buffer_ptr->thickness = thickness;
		s_data.circle_vertex_buffer_ptr->fade = fade;
		s_data.circle_vertex_buffer_ptr->entityID = entityID;
		s_data.circle_vertex_buffer_ptr++;
	}
	
	s_data.circle_index_count += 6;
	
	s_data.stats.quad_counts++;
}

void renderer2D::draw_line(const glm::vec3& p0, const glm::vec3& p1, const glm::vec4& color, int entityID)
{
	s_data.line_vertex_buffer_ptr->position = p0;
	s_data.line_vertex_buffer_ptr->color = color;
	s_data.line_vertex_buffer_ptr->entityID = entityID;
	s_data.line_vertex_buffer_ptr++;
	
	s_data.line_vertex_buffer_ptr->position = p1;
	s_data.line_vertex_buffer_ptr->color = color;
	s_data.line_vertex_buffer_ptr->entityID = entityID;
	s_data.line_vertex_buffer_ptr++;
	
	s_data.line_vertex_count += 2;
}

void renderer2D::draw_rect(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color, int entityID)
{
	glm::vec3 p0 = glm::vec3(position.x - size.x * 0.5f, position.y - size.y * 0.5f, position.z);
	glm::vec3 p1 = glm::vec3(position.x + size.x * 0.5f, position.y - size.y * 0.5f, position.z);
	glm::vec3 p2 = glm::vec3(position.x + size.x * 0.5f, position.y + size.y * 0.5f, position.z);
	glm::vec3 p3 = glm::vec3(position.x - size.x * 0.5f, position.y + size.y * 0.5f, position.z);

	draw_line(p0, p1, color, entityID);
	draw_line(p1, p2, color, entityID);
	draw_line(p2, p3, color, entityID);
	draw_line(p3, p0, color, entityID);
}

void renderer2D::draw_rect(const glm::mat4& transform, const glm::vec4& color, int entityID)
{
	glm::vec3 line_vertices[4];
	for (size_t i = 0; i < 4; i++)
		line_vertices[i] = transform * s_data.quad_vertex_positions[i];

	draw_line(line_vertices[0], line_vertices[1], color, entityID);
	draw_line(line_vertices[1], line_vertices[2], color, entityID);
	draw_line(line_vertices[2], line_vertices[3], color, entityID);
	draw_line(line_vertices[3], line_vertices[0], color, entityID);
}

void renderer2D::draw_sprite(const glm::mat4& transform, const sprite_renderer_component& src, int entityID)
{
	if (src.texture)
	{
		ref<texture2D> texture = asset_manager::get_asset<texture2D>(src.texture);
		draw_quad(transform, texture, src.tiling_factor, src.color, entityID);
	}
	else
	{
		draw_quad(transform, src.color, entityID);
	}
}

void renderer2D::draw_string(const std::string& string, ref<font> fnt, const glm::mat4& transform, const text_params& params, int entityID)
{
	WHP_PROFILE_FUNCTION();
	if (!fnt)
	{
		WHP_CORE_ERROR("[Renderer2D] Null font!");
		return;
	}
	const auto& font_geometry = fnt->get_msdf_data()->font_geometry;
	const auto& metrics = font_geometry.getMetrics();
	ref<texture2D> font_atlas = fnt->get_atlas_texture();

	s_data.font_atlas_texture = font_atlas;

	double x = 0.0;
	double fs_scale = 1.0 / (metrics.ascenderY - metrics.descenderY);
	double y = 0.0;

	const float space_glyph_advance = (float)font_geometry.getGlyph(' ')->getAdvance();

	for (size_t i = 0; i < string.size(); i++)
	{
		char character = string[i];
		if (character == '\r')
			continue;

		if (character == '\n')
		{
			x = 0;
			y -= fs_scale * metrics.lineHeight + params.line_spacing;
			continue;
		}

		if (character == ' ')
		{
			float advance = space_glyph_advance;
			if (i < string.size() - 1)
			{
				char next_character = string[i + 1];
				double d_advance;
				font_geometry.getAdvance(d_advance, character, next_character);
				advance = (float)d_advance;
			}

			x += fs_scale * advance + params.kerning;
			continue;
		}

		if (character == '\t')
		{
			// is this right?
			x += 4.0f * (fs_scale * space_glyph_advance + params.kerning);
			continue;
		}
		auto glyph = font_geometry.getGlyph(character);
		if (!glyph)
			glyph = font_geometry.getGlyph('?');
		if (!glyph)
			return;

		if (character == '\t')
			glyph = font_geometry.getGlyph(' ');

		double al, ab, ar, at;
		glyph->getQuadAtlasBounds(al, ab, ar, at);
		glm::vec2 texture_coord_min((float)al, (float)ab);
		glm::vec2 texture_coord_max((float)ar, (float)at);

		double pl, pb, pr, pt;
		glyph->getQuadPlaneBounds(pl, pb, pr, pt);
		glm::vec2 quad_min((float)pl, (float)pb);
		glm::vec2 quad_max((float)pr, (float)pt);

		quad_min *= fs_scale, quad_max *= fs_scale;
		quad_min += glm::vec2(x, y);
		quad_max += glm::vec2(x, y);

		float texel_width = 1.0f / font_atlas->get_width();
		float texel_height = 1.0f / font_atlas->get_height();
		texture_coord_min *= glm::vec2(texel_width, texel_height);
		texture_coord_max *= glm::vec2(texel_width, texel_height);

		// render here
		s_data.text_vertex_buffer_ptr->position = transform * glm::vec4(quad_min, 0.0f, 1.0f);
		s_data.text_vertex_buffer_ptr->color = params.color;
		s_data.text_vertex_buffer_ptr->texture_coord = texture_coord_min;
		s_data.text_vertex_buffer_ptr->entityID = entityID;
		s_data.text_vertex_buffer_ptr++;

		s_data.text_vertex_buffer_ptr->position = transform * glm::vec4(quad_min.x, quad_max.y, 0.0f, 1.0f);
		s_data.text_vertex_buffer_ptr->color = params.color;
		s_data.text_vertex_buffer_ptr->texture_coord = { texture_coord_min.x, texture_coord_max.y };
		s_data.text_vertex_buffer_ptr->entityID = entityID;
		s_data.text_vertex_buffer_ptr++;

		s_data.text_vertex_buffer_ptr->position = transform * glm::vec4(quad_max, 0.0f, 1.0f);
		s_data.text_vertex_buffer_ptr->color = params.color;
		s_data.text_vertex_buffer_ptr->texture_coord = texture_coord_max;
		s_data.text_vertex_buffer_ptr->entityID = entityID;
		s_data.text_vertex_buffer_ptr++;

		s_data.text_vertex_buffer_ptr->position = transform * glm::vec4(quad_max.x, quad_min.y, 0.0f, 1.0f);
		s_data.text_vertex_buffer_ptr->color = params.color;
		s_data.text_vertex_buffer_ptr->texture_coord = { texture_coord_max.x, texture_coord_min.y };
		s_data.text_vertex_buffer_ptr->entityID = entityID;
		s_data.text_vertex_buffer_ptr++;

		s_data.text_index_count += 6;
		s_data.stats.quad_counts++;

		if (i < string.size() - 1)
		{
			double advance = glyph->getAdvance();
			char next_character = string[i + 1];
			font_geometry.getAdvance(advance, character, next_character);

			x += fs_scale * advance + params.kerning;
		}
	}
}

void renderer2D::draw_string(const std::string& string, const glm::mat4& transform, const text_component& component, int entityID)
{
	draw_string(string, 
		component.font ? std::static_pointer_cast<font>(project::get_active()->get_asset_manager()->get_asset(component.font)) : font::get_default(), 
		transform, 
		{ component.color, component.kerning, component.line_spacing }, 
		entityID);
}

void renderer2D::set_line_width(float width)
{
	render_command::set_line_width(width);
}

void renderer2D::reset_stats()
{
	memset(&s_data.stats, 0, sizeof(renderer2D::statistics));
}

renderer2D::statistics renderer2D::get_stats()
{
	return s_data.stats;
}

void renderer2D::start_batch()
{
	s_data.quad_index_count = 0;
	s_data.quad_vertex_buffer_ptr = s_data.quad_vertex_buffer_base;

	s_data.circle_index_count = 0;
	s_data.circle_vertex_buffer_ptr = s_data.circle_vertex_buffer_base;

	s_data.line_vertex_count = 0;
	s_data.line_vertex_buffer_ptr = s_data.line_vertex_buffer_base;

	s_data.text_index_count = 0;
	s_data.text_vertex_buffer_ptr = s_data.text_vertex_buffer_base;

	s_data.texture_slot_index = 1;
}

void renderer2D::next_batch()
{
	flush();
	start_batch();
}

_WHIP_END
