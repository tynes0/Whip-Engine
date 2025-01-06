#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/memory.h>
#include <Whip/Core/buffer.h>
#include <Whip/Asset/asset.h>

#include <string>

_WHIP_START

enum class image_format
{
	None = 0, 
	R8,
	RGB8,
	RGBA8,
	RGBA32F
};

struct texture_specification
{
	uint32_t width = 1;
	uint32_t height = 1;
	image_format format = image_format::RGBA8;
	bool generate_mips = true;
};

class texture : public asset
{
public:
	virtual ~texture() = default;

	virtual const texture_specification& get_specification() const = 0;

	virtual uint32_t get_width() const = 0;
	virtual uint32_t get_height() const = 0;

	virtual renderer_id_t get_renderer_id() const = 0;

	virtual void set_data(raw_buffer data) = 0;
	virtual raw_buffer get_data() const = 0;
	virtual void bind(uint32_t slot = 0) const = 0;
	virtual bool is_loaded() const = 0;
	virtual bool operator==(const texture& other) const = 0;
};

typedef int flip_direction_t;

enum : flip_direction_t
{
	flip_direction_none			= WHP_BIT(0),
	flip_direction_horizontal	= WHP_BIT(1),
	flip_direction_vertical		= WHP_BIT(2)
};

class texture2D : public texture
{
public:
	static ref<texture2D> create(const texture_specification& specification, raw_buffer data = raw_buffer());

	static ref<texture2D> create_from_coords(const ref<texture2D>& atlas, const glm::vec2& coords, const glm::vec2& cell_size, 
		const glm::vec2& pixel_size_between_sprites = { 0.0f, 0.0f }, const glm::vec2& sprite_size = { 1.0f, 1.0f });
	static void flip_texture_buffer(raw_buffer& buffer, int width, int height, int channels, flip_direction_t direction);
	static asset_type get_static_type() { return asset_type::texture2D; }
	virtual asset_type get_type() const override { return get_static_type(); }
};

_WHIP_END
