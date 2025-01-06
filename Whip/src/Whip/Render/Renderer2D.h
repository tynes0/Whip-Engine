#pragma once

#include "camera.h"
#include "orthographic_camera.h"
#include "editor_camera.h"
#include "Texture.h"
#include "SubTexture2D.h"
#include "font.h"

#include <Whip/Core/memory.h>
#include <Whip/Scene/components.h>

_WHIP_START

class renderer2D
{
public:
	struct text_params
	{
		glm::vec4 color{ 1.0f };
		float kerning = 0.0f;
		float line_spacing = 0.0f;
	};

	static void init();
	static void shutdown();

	static void begin_scene(const orthographic_camera& camera);
	static void begin_scene(const camera& cam, const glm::mat4& transform);
	static void begin_scene(const editor_camera& cam);
	static void end_scene();

	static void flush();

	static void draw_quad(const glm::mat4& transform, const glm::vec4& color, int entityID = -1);
	static void draw_quad(const glm::mat4& transform, const ref<texture2D>& tex, float tiling_factor = 1.0f, const glm::vec4& tint_color = glm::vec4(1.0f), int entityID = -1);
	static void draw_quad(const glm::mat4& transform, const ref<sub_texture2D>& sub_tex, float tiling_factor = 1.0f, const glm::vec4& tint_color = glm::vec4(1.0f), int entityID = -1);

	static void draw_quad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color);
	static void draw_quad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color);
	static void draw_quad(const glm::vec2& position, const glm::vec2& size, const ref<texture2D>& tex, float tiling_factor = 1.0f, const glm::vec4& tint_color = glm::vec4(1.0f));
	static void draw_quad(const glm::vec3& position, const glm::vec2& size, const ref<texture2D>& tex, float tiling_factor = 1.0f, const glm::vec4& tint_color = glm::vec4(1.0f));
	static void draw_quad(const glm::vec2& position, const glm::vec2& size, const ref<sub_texture2D>& sub_tex, float tiling_factor = 1.0f, const glm::vec4& tint_color = glm::vec4(1.0f));
	static void draw_quad(const glm::vec3& position, const glm::vec2& size, const ref<sub_texture2D>& sub_tex, float tiling_factor = 1.0f, const glm::vec4& tint_color = glm::vec4(1.0f));

	static void draw_rotated_quad(const glm::vec2& position, const glm::vec2& size, float rotation, const glm::vec4& color);
	static void draw_rotated_quad(const glm::vec3& position, const glm::vec2& size, float rotation, const glm::vec4& color);
	static void draw_rotated_quad(const glm::vec2& position, const glm::vec2& size, float rotation, const ref<texture2D>& tex, float tiling_factor = 1.0f, const glm::vec4& tint_color = glm::vec4(1.0f));
	static void draw_rotated_quad(const glm::vec3& position, const glm::vec2& size, float rotation, const ref<texture2D>& tex, float tiling_factor = 1.0f, const glm::vec4& tint_color = glm::vec4(1.0f));
	static void draw_rotated_quad(const glm::vec2& position, const glm::vec2& size, float rotation, const ref<sub_texture2D>& sub_tex, float tiling_factor = 1.0f, const glm::vec4& tint_color = glm::vec4(1.0f));
	static void draw_rotated_quad(const glm::vec3& position, const glm::vec2& size, float rotation, const ref<sub_texture2D>& sub_tex, float tiling_factor = 1.0f, const glm::vec4& tint_color = glm::vec4(1.0f));

	static void draw_circle(const glm::mat4& transform, const glm::vec4& color, float thickness = 1.0f, float fade = 0.005f, int entityID = -1);

	static void draw_line(const glm::vec3& p0, const glm::vec3& p1, const glm::vec4& color, int entityID = -1);

	static void draw_rect(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color, int entityID = -1);
	static void draw_rect(const glm::mat4& transform, const glm::vec4& color, int entityID = -1);

	static void draw_sprite(const glm::mat4& transform, const sprite_renderer_component& src, int entityID);

	static void draw_string(const std::string& string, ref<font> fnt, const glm::mat4& transform, const text_params& params, int entityID = -1);
	static void draw_string(const std::string& string, const glm::mat4& transform, const text_component& component, int entityID = -1);

	static void set_line_width(float width);

	// statistics
	struct statistics
	{
		uint32_t draw_calls = 0;
		uint32_t quad_counts = 0;

		uint32_t get_total_vertex_count() const { return quad_counts * 4; }
		uint32_t get_total_index_count() const { return quad_counts * 6; }
	};

	static void reset_stats();
	static statistics get_stats();

private:
	static void start_batch();
	static void next_batch();
};

_WHIP_END
