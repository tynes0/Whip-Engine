#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Render/Texture.h>
#include <glm/glm.hpp>

_WHIP_START

class SubTexture2D
{
public:
	SubTexture2D(const Ref<Texture2D>& texture, const glm::vec2& min, const glm::vec2& max);

	const Ref<Texture2D>& GetTexture() const;
	const glm::vec2* GetTextureCoords() const;

	static Ref<SubTexture2D> CreateFromCoords(const Ref<Texture2D>& texture, const glm::vec2& coords, const glm::vec2& cellSize, const glm::vec2& pixelSizeBetweenSprites = {0.0f, 0.0f}, const glm::vec2& spriteSize = {1.0f, 1.0f});
private:
	Ref<Texture2D> m_Texture;
	glm::vec2 m_TextureCoords[4];
};

_WHIP_END
