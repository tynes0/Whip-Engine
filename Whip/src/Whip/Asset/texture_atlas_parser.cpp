#include "whippch.h"
//#include "texture_atlas_parser.h"
//
//#include <queue>
//
//_WHIP_START
//
////void texture_atlas_parser::detect_sprites_grid(const glm::vec2& cell_size, const glm::vec2& pixel_spacing)
////{
////	if (!m_spritesheet)
////	{
////		WHP_CORE_ERROR("Spritesheet is not valid!");
////		return;
////	}
////
////	uint32_t sheet_width = m_spritesheet->get_width();
////	uint32_t sheet_height = m_spritesheet->get_height();
////
////	for (float y = 0.0f; y + cell_size.y <= sheet_height; y += cell_size.y + pixel_spacing.y)
////		for (float x = 0.0f; x + cell_size.x <= sheet_width; x += cell_size.x + pixel_spacing.x)
////			extract_sprite({ x, y }, cell_size, pixel_spacing);
////}
//
//void texture_atlas_parser::detect_sprites_auto() {
//	if (!m_spritesheet) {
//		WHP_CORE_ERROR("Spritesheet is not valid!");
//		return;
//	}
//
//	uint32_t sheet_width = m_spritesheet->get_width();
//	uint32_t sheet_height = m_spritesheet->get_height();
//	raw_buffer data = m_spritesheet->get_data();
//
//	if (!data || data.size < sheet_width * sheet_height * 4) {
//		WHP_CORE_ERROR("Invalid spritesheet data!");
//		return;
//	}
//
//	uint8_t* pixels = data.data;
//	const uint8_t alpha_threshold = 10; // Threshold to detect transparency
//	const uint32_t min_sprite_size = 8; // Minimum sprite dimension in pixels
//	std::vector<std::vector<bool>> visited(sheet_height, std::vector<bool>(sheet_width, false));
//
//	for (uint32_t y = 0; y < sheet_height; ++y) {
//		for (uint32_t x = 0; x < sheet_width; ++x) {
//			if (visited[y][x]) continue; // Skip already processed pixels
//
//			uint8_t* pixel = &pixels[(y * sheet_width + x) * 4]; // RGBA format
//			if (pixel[3] > alpha_threshold) { // Non-transparent pixel detected
//				glm::vec2 start_coords = { x, y };
//				glm::vec2 sprite_size = find_non_empty_area(start_coords, pixels, sheet_width, sheet_height, alpha_threshold, visited);
//
//				// Validate sprite size
//				if (sprite_size.x < min_sprite_size || sprite_size.y < min_sprite_size) {
//					WHP_CORE_WARN("Sprite at ({}, {}) is too small (size: {}, {}). Skipping...", start_coords.x, start_coords.y, sprite_size.x, sprite_size.y);
//					continue;
//				}
//
//				WHP_CORE_INFO("Sprite detected at ({}, {}), size: ({}, {})", start_coords.x, start_coords.y, sprite_size.x, sprite_size.y);
//				extract_sprite(start_coords, sprite_size);
//			}
//		}
//	}
//}
//
//
//
//void texture_atlas_parser::extract_sprite(const glm::vec2& coords, const glm::vec2& sprite_size)
//{
//	uint32_t atlas_width = m_spritesheet->get_width();
//	uint32_t atlas_height = m_spritesheet->get_height();
//
//	uint32_t sprite_width = static_cast<uint32_t>(sprite_size.x);
//	uint32_t sprite_height = static_cast<uint32_t>(sprite_size.y);
//
//	// Boundary check
//	if (static_cast<uint32_t>(coords.y) + sprite_height > atlas_height ||
//		static_cast<uint32_t>(coords.x) + sprite_width > atlas_width) {
//		WHP_CORE_ERROR("Sprite dimensions exceed atlas boundaries! Skipping...");
//		return;
//	}
//
//	raw_buffer atlas_data = m_spritesheet->get_data();
//	raw_buffer sprite_data(sprite_width * sprite_height * 4); // RGBA format
//
//	for (uint32_t y = 0; y < sprite_height; ++y) {
//		uint32_t atlas_row_start = (static_cast<uint32_t>(coords.y) + y) * atlas_width * 4 + static_cast<uint32_t>(coords.x) * 4;
//		uint32_t sprite_row_start = y * sprite_width * 4;
//
//		// Memory boundary check
//		if (atlas_row_start + sprite_width * 4 > atlas_data.size || sprite_row_start + sprite_width * 4 > sprite_data.size) {
//			WHP_CORE_ERROR("Memory copy exceeds buffer size! Skipping row...");
//			continue;
//		}
//
//		memcpy(sprite_data.data + sprite_row_start, atlas_data.data + atlas_row_start, sprite_width * 4);
//	}
//
//	// Create new texture
//	texture_specification spec;
//	spec.width = sprite_width;
//	spec.height = sprite_height;
//	spec.format = image_format::RGBA8;
//
//	ref<texture2D> sprite = texture2D::create(spec, sprite_data);
//	if (sprite) {
//		m_sprites.push_back(sprite);
//	}
//	else {
//		WHP_CORE_WARN("Failed to create sprite at coords: ({}, {})", coords.x, coords.y);
//	}
//}
//
//glm::vec2 texture_atlas_parser::find_non_empty_area(const glm::vec2& start_coords, uint8_t* pixels, uint32_t sheet_width, uint32_t sheet_height, uint8_t alpha_threshold, std::vector<std::vector<bool>>& visited) {
//	glm::vec2 min_coords = start_coords;
//	glm::vec2 max_coords = start_coords;
//
//	std::queue<glm::vec2> queue;
//	queue.push(start_coords);
//	visited[static_cast<uint32_t>(start_coords.y)][static_cast<uint32_t>(start_coords.x)] = true;
//
//	while (!queue.empty()) {
//		glm::vec2 current = queue.front();
//		queue.pop();
//
//		// Update bounds
//		min_coords.x = std::min(min_coords.x, current.x);
//		min_coords.y = std::min(min_coords.y, current.y);
//		max_coords.x = std::max(max_coords.x, current.x);
//		max_coords.y = std::max(max_coords.y, current.y);
//
//		// Log bounds for debugging
//		WHP_CORE_DEBUG("Updating bounds: min({}, {}), max({}, {})", min_coords.x, min_coords.y, max_coords.x, max_coords.y);
//
//		// Check neighbors
//		const std::vector<glm::vec2> directions = { {-1, 0}, {1, 0}, {0, -1}, {0, 1} };
//		for (const glm::vec2& dir : directions) {
//			glm::vec2 neighbor = current + dir;
//			uint32_t nx = static_cast<uint32_t>(neighbor.x);
//			uint32_t ny = static_cast<uint32_t>(neighbor.y);
//
//			if (nx < 0 || ny < 0 || nx >= sheet_width || ny >= sheet_height || visited[ny][nx]) continue;
//
//			uint8_t* pixel = &pixels[(ny * sheet_width + nx) * 4];
//			if (pixel[3] > alpha_threshold) {
//				visited[ny][nx] = true;
//				queue.push(neighbor);
//			}
//		}
//	}
//
//	return { max_coords.x - min_coords.x + 1, max_coords.y - min_coords.y + 1 };
//}
//
//_WHIP_END
