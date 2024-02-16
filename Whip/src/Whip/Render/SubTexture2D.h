#pragma once

#include "Whip/Core/Core.h"
#include "Texture.h"
#include <glm/glm.hpp>

_WHIP_START

class sub_texture2D
{
public:
	sub_texture2D(const ref<texture2D>& _texture, const glm::vec2& min, const glm::vec2& max);

	const ref<texture2D> get_texture();
	const glm::vec2* get_texture_coords();

	static ref<sub_texture2D> create_from_coords(const ref<texture2D>& _texture, const glm::vec2& coords, const glm::vec2& cell_size, const glm::vec2& pixel_size_between_sprites = {0.0f, 0.0f}, const glm::vec2& sprite_size = {1.0f, 1.0f});
private:
	ref<texture2D> m_texture;
	glm::vec2 m_texture_coords[4];
};

_WHIP_END