#include "WhipPch.h"
#include <Whip/Render/SubTexture2D.h>

_WHIP_START

SubTexture2D::SubTexture2D(const Ref<Texture2D>& texture, const glm::vec2& min, const glm::vec2& max) : m_Texture(texture)
{
	m_TextureCoords[0] = { min.x, min.y };
	m_TextureCoords[1] = { max.x, min.y };
	m_TextureCoords[2] = { max.x, max.y };
	m_TextureCoords[3] = { min.x, max.y };
}

const Ref<Texture2D>& SubTexture2D::GetTexture() const
{
	return m_Texture;
}

const glm::vec2* SubTexture2D::GetTextureCoords() const
{
	return m_TextureCoords;
}

Ref<SubTexture2D> SubTexture2D::CreateFromCoords(const Ref<Texture2D>& texture, const glm::vec2& coords, const glm::vec2& cellSize, const glm::vec2& pixelSizeBetweenSprites, const glm::vec2& spriteSize)
{
	glm::vec2 emptyPixelSize = { pixelSizeBetweenSprites.x * coords.x, pixelSizeBetweenSprites.y * coords.y };
	glm::vec2 min = { ((coords.x * cellSize.x) + emptyPixelSize.x) / texture->GetWidth(), ((coords.y * cellSize.y) + emptyPixelSize.y) / texture->GetHeight() };
	glm::vec2 max = { (((coords.x + spriteSize.x) * cellSize.x) + emptyPixelSize.x) / texture->GetWidth(), (((coords.y + spriteSize.y) * cellSize.y) + emptyPixelSize.y) / texture->GetHeight() };
	return MakeRef<SubTexture2D>(texture, min, max);
}

_WHIP_END
