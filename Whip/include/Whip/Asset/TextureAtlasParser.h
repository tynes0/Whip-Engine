#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Render/Texture.h>
#include <vector>
#include <glm/glm.hpp>
#include <queue>

_WHIP_START

// NOT WORKS FINE FOR NOW - SOME SPRITIES WERE CUT OFF ON THE LEFT SIDE OR RIGHT SIDE + NOT OPTIMIZED (I'LL ADD MULTI-THREADING)
class TextureAtlasParser {
public:
	// Constructor: Initialize with a spritesheet
	TextureAtlasParser(const Ref<Texture2D>& spritesheet)
		: m_Spritesheet(spritesheet) {
	}

	// Detect sprites automatically based on non-empty areas
	void DetectSpritesAuto() {
		if (!m_Spritesheet) {
			WHP_CORE_ERROR("Spritesheet is not valid!");
			return;
		}

		uint32_t sheetWidth = m_Spritesheet->GetWidth();
		uint32_t sheetHeight = m_Spritesheet->GetHeight();
		RawBuffer data = m_Spritesheet->GetData();

		if (!data || data.m_Size < sheetWidth * sheetHeight * 4) {
			WHP_CORE_ERROR("Invalid spritesheet data!");
			return;
		}

		uint8_t* pixels = data.m_Data;
		const uint8_t alphaThreshold = 10; // Threshold to detect transparency
		const uint32_t minSpriteSize = 8; // Minimum sprite dimension in pixels
		std::vector<std::vector<bool>> visited(sheetHeight, std::vector<bool>(sheetWidth, false));

		for (uint32_t y = 0; y < sheetHeight; ++y) {
			for (uint32_t x = 0; x < sheetWidth; ++x) {
				if (visited[y][x]) continue; // Skip already processed pixels

				uint8_t* pixel = &pixels[(y * sheetWidth + x) * 4]; // RGBA format
				if (pixel[3] > alphaThreshold) { // Non-transparent pixel detected
					glm::vec2 startCoords = { x, y };
					glm::vec2 spriteSize = FindNonEmptyArea(startCoords, pixels, sheetWidth, sheetHeight, alphaThreshold, visited);

					// Validate sprite size
					if (spriteSize.x < minSpriteSize || spriteSize.y < minSpriteSize) {
						WHP_CORE_WARN("Sprite at ({}, {}) is too small (size: {}, {}). Skipping...", startCoords.x, startCoords.y, spriteSize.x, spriteSize.y);
						continue;
					}

					WHP_CORE_INFO("Sprite detected at ({}, {}), size: ({}, {})", startCoords.x, startCoords.y, spriteSize.x, spriteSize.y);
					ExtractSpriteSafe(startCoords, spriteSize);
				}
			}
		}
		data.Release();
	}

	// Get the detected sprites
	const std::vector<Ref<Texture2D>>& GetSprites() const {
		return m_Sprites;
	}

private:
	Ref<Texture2D> m_Spritesheet; // The source spritesheet
	std::vector<Ref<Texture2D>> m_Sprites; // Detected sprites

	// Helper function to extract a sprite from specific coordinates
	void ExtractSpriteSafe(const glm::vec2& coords, const glm::vec2& spriteSize) {
		uint32_t atlasWidth = m_Spritesheet->GetWidth();
		uint32_t atlasHeight = m_Spritesheet->GetHeight();

		uint32_t spriteWidth = static_cast<uint32_t>(spriteSize.x);
		uint32_t spriteHeight = static_cast<uint32_t>(spriteSize.y);

		// Boundary check
		if (static_cast<uint32_t>(coords.y) + spriteHeight > atlasHeight ||
			static_cast<uint32_t>(coords.x) + spriteWidth > atlasWidth) {
			WHP_CORE_ERROR("Sprite dimensions exceed atlas boundaries! Skipping...");
			return;
		}

		RawBuffer atlasData = m_Spritesheet->GetData();
		RawBuffer spriteData(spriteWidth * spriteHeight * 4); // RGBA format

		for (uint32_t y = 0; y < spriteHeight; ++y) {
			uint32_t atlasRowStart = (static_cast<uint32_t>(coords.y) + y) * atlasWidth * 4 + static_cast<uint32_t>(coords.x) * 4;
			uint32_t spriteRowStart = y * spriteWidth * 4;

			// Memory boundary check
			if (atlasRowStart + spriteWidth * 4 > atlasData.m_Size || spriteRowStart + spriteWidth * 4 > spriteData.m_Size) {
				WHP_CORE_ERROR("Memory copy exceeds buffer size! Skipping row...");
				continue;
			}

			memcpy(spriteData.m_Data + spriteRowStart, atlasData.m_Data + atlasRowStart, spriteWidth * 4);
		}
		atlasData.Release();
		// Create new Texture
		TextureSpecification spec;
		spec.m_Width = spriteWidth;
		spec.m_Height = spriteHeight;
		spec.m_Format = ImageFormat::Rgba8;

		Ref<Texture2D> sprite = Texture2D::Create(spec, spriteData);
		if (sprite) {
			m_Sprites.push_back(sprite);
		}
		else {
			WHP_CORE_WARN("Failed to create sprite at coords: ({}, {})", coords.x, coords.y);
		}
		spriteData.Release();
	}

	// Find the bounds of a non-empty area
	glm::vec2 FindNonEmptyArea(const glm::vec2& startCoords, uint8_t* pixels, uint32_t sheetWidth, uint32_t sheetHeight, uint8_t alphaThreshold, std::vector<std::vector<bool>>& visited) {
		glm::vec2 minCoords = startCoords;
		glm::vec2 maxCoords = startCoords;

		std::queue<glm::vec2> queue;
		queue.push(startCoords);
		visited[static_cast<uint32_t>(startCoords.y)][static_cast<uint32_t>(startCoords.x)] = true;

		while (!queue.empty()) {
			glm::vec2 current = queue.front();
			queue.pop();

			// Update bounds
			minCoords.x = std::min(minCoords.x, current.x);
			minCoords.y = std::min(minCoords.y, current.y);
			maxCoords.x = std::max(maxCoords.x, current.x);
			maxCoords.y = std::max(maxCoords.y, current.y);

			// Log bounds for debugging
			WHP_CORE_INFO("Updating bounds: min({}, {}), max({}, {})", minCoords.x, minCoords.y, maxCoords.x, maxCoords.y);

			// Check neighbors
			const std::vector<glm::vec2> directions = { {-1, 0}, {1, 0}, {0, -1}, {0, 1} };
			for (const glm::vec2& dir : directions) {
				glm::vec2 neighbor = current + dir;
				uint32_t nx = static_cast<uint32_t>(neighbor.x);
				uint32_t ny = static_cast<uint32_t>(neighbor.y);

				if (nx < 0 || ny < 0 || nx >= sheetWidth || ny >= sheetHeight || visited[ny][nx]) continue;

				uint8_t* pixel = &pixels[(ny * sheetWidth + nx) * 4];
				if (pixel[3] > alphaThreshold) {
					visited[ny][nx] = true;
					queue.push(neighbor);
				}
			}
		}

		return { maxCoords.x - minCoords.x + 1, maxCoords.y - minCoords.y + 1 };
	}
};

_WHIP_END
