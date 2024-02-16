#include "whippch.h"
#include "SubTexture2D.h"

_WHIP_START

sub_texture2D::sub_texture2D(const ref<texture2D>& _texture, const glm::vec2& min, const glm::vec2& max) : m_texture(_texture)
{
	m_texture_coords[0] = { min.x, min.y };
	m_texture_coords[1] = { max.x, min.y };
	m_texture_coords[2] = { max.x, max.y };
	m_texture_coords[3] = { min.x, max.y };
}

const ref<texture2D> sub_texture2D::get_texture()
{
	return m_texture;
}

const glm::vec2* sub_texture2D::get_texture_coords()
{
	return m_texture_coords;
}

ref<sub_texture2D> sub_texture2D::create_from_coords(const ref<texture2D>& _texture, const glm::vec2& coords, const glm::vec2& cell_size, const glm::vec2& pixel_size_between_sprites, const glm::vec2& sprite_size)
{
	glm::vec2 empty_pixel_size = { pixel_size_between_sprites.x * coords.x, pixel_size_between_sprites.y * coords.y };
	glm::vec2 min = { ((coords.x * cell_size.x) + empty_pixel_size.x) / _texture->get_width(), ((coords.y * cell_size.y) + empty_pixel_size.y) / _texture->get_height() };
	glm::vec2 max = { (((coords.x + sprite_size.x) * cell_size.x) + empty_pixel_size.x) / _texture->get_width(), (((coords.y + sprite_size.y) * cell_size.y) + empty_pixel_size.y) / _texture->get_height() };
	return make_ref<sub_texture2D>(_texture, min, max);
}

_WHIP_END
