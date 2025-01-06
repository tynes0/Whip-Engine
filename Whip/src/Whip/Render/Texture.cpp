#include "whippch.h"
#include "Texture.h"

#include <Whip/Render/Renderer.h>
#include <Platform/OpenGL/OpenGLTexture.h>

_WHIP_START

ref<texture2D> texture2D::create(const texture_specification& specification, raw_buffer data)
{
	switch (renderer::get_API())
	{
	case render_API::API::none:		WHP_CORE_ASSERT(false, "RandererAPI is none!"); return nullptr;
	case render_API::API::opengl:	return make_ref<opengl_texture2D>(specification, data);
	}

	WHP_CORE_ASSERT(false, "Unknown RendererAPI");
	return nullptr;
}

ref<texture2D> texture2D::create_from_coords(const ref<texture2D>& atlas, const glm::vec2& coords, const glm::vec2& cell_size, const glm::vec2& pixel_size_between_sprites, const glm::vec2& sprite_size)
{
	// Koordinatlarý hesapla
	glm::vec2 empty_pixel_size = { pixel_size_between_sprites.x * coords.x, pixel_size_between_sprites.y * coords.y };
	uint32_t atlas_width = atlas->get_width();
	uint32_t atlas_height = atlas->get_height();

	glm::vec2 min = { ((coords.x * cell_size.x) + empty_pixel_size.x),
					  ((coords.y * cell_size.y) + empty_pixel_size.y) };
	glm::vec2 max = { (((coords.x + sprite_size.x) * cell_size.x) + empty_pixel_size.x),
					  (((coords.y + sprite_size.y) * cell_size.y) + empty_pixel_size.y) };

	uint32_t sprite_width = static_cast<uint32_t>(max.x - min.x);
	uint32_t sprite_height = static_cast<uint32_t>(max.y - min.y);

	// Atlasýn ham verisini al
	raw_buffer atlas_data = atlas->get_data();

	// Yeni sprite için buffer oluþtur
	raw_buffer sprite_data(sprite_width * sprite_height * 4); // RGBA8 formatýný varsayýyoruz

	// Veriyi atlas'tan yeni sprite'a kopyala
	for (uint32_t y = 0; y < sprite_height; y++)
	{
		uint32_t atlas_row_start = (static_cast<uint32_t>(min.y) + y) * atlas_width * 4 + static_cast<uint32_t>(min.x) * 4;
		uint32_t sprite_row_start = y * sprite_width * 4;

		memcpy(sprite_data.data + sprite_row_start, atlas_data.data + atlas_row_start, sprite_width * 4);
	}

	// Yeni texture oluþtur
	texture_specification spec;
	spec.width = sprite_width;
	spec.height = sprite_height;
	spec.format = image_format::RGBA8;

	return texture2D::create(spec, sprite_data);
}


void texture2D::flip_texture_buffer(raw_buffer& buffer, int width, int height, int channels, flip_direction_t direction)
{
	if (!buffer || buffer.size < uint64_t(width * height * channels))
	{
		WHP_CORE_ERROR("[Texture 2D] Invalid buffer size or data!");
		return;
	}

	uint8_t* data = buffer.data;

	if (direction & flip_direction_horizontal)
	{
		size_t bytes_per_row = (size_t)width * channels;
		raw_buffer temp(2048);
		raw_buffer bytes = buffer;

		for (int row = 0; row < height; row++) 
		{
			raw_buffer row_data(bytes.data + row * bytes_per_row, 1);
			size_t left = 0;
			size_t right = size_t(width - 1);

			while (left < right)
			{
				uint8_t* left_pixel = row_data.data + left * channels;
				uint8_t* right_pixel = row_data.data + right * channels;

				size_t bytes_copy = (channels < temp.size) ? channels : sizeof(temp);

				memcpy(temp.data, left_pixel, bytes_copy);
				memcpy(left_pixel, right_pixel, bytes_copy);
				memcpy(right_pixel, temp.data, bytes_copy);

				left++;
				right--;
			}
		}
	}

	if (direction & flip_direction_vertical)
	{
		int row;
		size_t bytes_per_row = (size_t)width * channels;
		raw_buffer temp(2048);
		raw_buffer bytes = buffer;

		for (row = 0; row < (height >> 1); row++)
		{
			raw_buffer row0(bytes.data + row * bytes_per_row, 1);
			raw_buffer row1(bytes.data + (height - row - 1) * bytes_per_row, 1);
			size_t bytes_left = bytes_per_row;
			while (bytes_left) 
			{
				size_t bytes_copy = (bytes_left < temp.size) ? bytes_left : sizeof(temp);
				memcpy(temp.data, row0.data, bytes_copy);
				memcpy(row0.data, row1.data, bytes_copy);
				memcpy(row1.data, temp.data, bytes_copy);
				row0.data += bytes_copy;
				row1.data += bytes_copy;
				bytes_left -= bytes_copy;
			}
		}
	}
}

_WHIP_END
