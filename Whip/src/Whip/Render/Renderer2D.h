#pragma once

#include <Whip/Render/OrthographicCameraController.h>
#include <Whip/Render/Texture.h>
#include <Whip/Core/Memory.h>

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
	static void draw_quad(const glm::vec2& position, const glm::vec2& size, const ref<texture2D>& texture, float tiling_factor = 1.0f, const glm::vec4& tint_color = glm::vec4(1.0f));
	static void draw_quad(const glm::vec3& position, const glm::vec2& size, const ref<texture2D>& texture, float tiling_factor = 1.0f, const glm::vec4& tint_color = glm::vec4(1.0f));

	static void draw_rotated_quad(const glm::vec2& position, const glm::vec2& size, float rotation, const glm::vec4& color);
	static void draw_rotated_quad(const glm::vec3& position, const glm::vec2& size, float rotation, const glm::vec4& color);
	static void draw_rotated_quad(const glm::vec2& position, const glm::vec2& size, float rotation, const ref<texture2D>& texture, float tiling_factor = 1.0f, const glm::vec4& tint_color = glm::vec4(1.0f));
	static void draw_rotated_quad(const glm::vec3& position, const glm::vec2& size, float rotation, const ref<texture2D>& texture, float tiling_factor = 1.0f, const glm::vec4& tint_color = glm::vec4(1.0f));
};

_WHIP_END