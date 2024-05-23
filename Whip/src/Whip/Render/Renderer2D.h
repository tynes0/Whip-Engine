#pragma once

#include <Whip/Render/OrthographicCameraController.h>
#include <Whip/Render/Texture.h>
#include <Whip/Render/SubTexture2D.h>

_WHIP_START

class renderer2D
{
public:
	static void init();
	static void shutdown();

	static void begin_scene(const orthographic_camera& camera);
	static void end_scene();

	static void flush();

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


	// statistics
	struct statistics
	{
		uint32_t draw_calls = 0;
		uint32_t quad_counts = 0;

		uint32_t get_total_vertex_count() { return quad_counts * 4; }
		uint32_t get_total_index_count() { return quad_counts * 6; }
	};

	static void reset_stats();
	static statistics get_stats();

private:
	static void flush_and_reset();
};

_WHIP_END