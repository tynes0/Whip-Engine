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

	static void draw_quad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color);
	static void draw_quad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color);
	static void draw_quad(const glm::vec2& position, const glm::vec2& size, const ref<texture2D>& texture);
	static void draw_quad(const glm::vec3& position, const glm::vec2& size, const ref<texture2D>& texture);
};

_WHIP_END