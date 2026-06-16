#pragma once

#include "Camera.h"
#include "OrthographicCamera.h"
#include "EditorCamera.h"
#include "Texture.h"
#include "SubTexture2D.h"
#include "Font.h"

#include <Whip/Core/Memory.h>
#include <Whip/Scene/Components.h>

_WHIP_START

class Renderer2D
{
public:
	struct TextParams
	{
		glm::vec4 m_Color{ 1.0f };
		float m_Kerning = 0.0f;
		float m_LineSpacing = 0.0f;
	};

	static void Init();
	static void Shutdown();

	static void BeginScene(const OrthographicCamera& camera);
	static void BeginScene(const Camera& cam, const glm::mat4& transform);
	static void BeginScene(const EditorCamera& cam);
	static void EndScene();

	static void Flush();

	static void DrawQuad(const glm::mat4& transform, const glm::vec4& color, int entityId = -1);
	static void DrawQuad(const glm::mat4& transform, const Ref<Texture2D>& tex, float tilingFactor = 1.0f, const glm::vec4& tintColor = glm::vec4(1.0f), int entityId = -1);
	static void DrawQuad(const glm::mat4& transform, const Ref<SubTexture2D>& subTexture, float tilingFactor = 1.0f, const glm::vec4& tintColor = glm::vec4(1.0f), int entityId = -1);

	static void DrawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color);
	static void DrawQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color);
	static void DrawQuad(const glm::vec2& position, const glm::vec2& size, const Ref<Texture2D>& tex, float tilingFactor = 1.0f, const glm::vec4& tintColor = glm::vec4(1.0f));
	static void DrawQuad(const glm::vec3& position, const glm::vec2& size, const Ref<Texture2D>& tex, float tilingFactor = 1.0f, const glm::vec4& tintColor = glm::vec4(1.0f));
	static void DrawQuad(const glm::vec2& position, const glm::vec2& size, const Ref<SubTexture2D>& subTexture, float tilingFactor = 1.0f, const glm::vec4& tintColor = glm::vec4(1.0f));
	static void DrawQuad(const glm::vec3& position, const glm::vec2& size, const Ref<SubTexture2D>& subTexture, float tilingFactor = 1.0f, const glm::vec4& tintColor = glm::vec4(1.0f));

	static void DrawRotatedQuad(const glm::vec2& position, const glm::vec2& size, float rotation, const glm::vec4& color);
	static void DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float rotation, const glm::vec4& color);
	static void DrawRotatedQuad(const glm::vec2& position, const glm::vec2& size, float rotation, const Ref<Texture2D>& tex, float tilingFactor = 1.0f, const glm::vec4& tintColor = glm::vec4(1.0f));
	static void DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float rotation, const Ref<Texture2D>& tex, float tilingFactor = 1.0f, const glm::vec4& tintColor = glm::vec4(1.0f));
	static void DrawRotatedQuad(const glm::vec2& position, const glm::vec2& size, float rotation, const Ref<SubTexture2D>& subTexture, float tilingFactor = 1.0f, const glm::vec4& tintColor = glm::vec4(1.0f));
	static void DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float rotation, const Ref<SubTexture2D>& subTexture, float tilingFactor = 1.0f, const glm::vec4& tintColor = glm::vec4(1.0f));

	static void DrawCircle(const glm::mat4& transform, const glm::vec4& color, float thickness = 1.0f, float fade = 0.005f, int entityId = -1);

	static void DrawLine(const glm::vec3& p0, const glm::vec3& p1, const glm::vec4& color, int entityId = -1);

	static void DrawRect(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color, int entityId = -1);
	static void DrawRect(const glm::mat4& transform, const glm::vec4& color, int entityId = -1);

	static void DrawSprite(const glm::mat4& transform, const SpriteRendererComponent& src, int entityId);

	static void DrawString(const std::string& text, Ref<Font> font, const glm::mat4& transform, const TextParams& params, int entityId = -1);
	static void DrawString(const std::string& text, const glm::mat4& transform, const TextComponent& component, int entityId = -1);

	static void SetLineWidth(float width);

	// Statistics
	struct Statistics
	{
		uint32_t m_DrawCalls = 0;
		uint32_t m_QuadCounts = 0;

		uint32_t GetTotalVertexCount() const { return m_QuadCounts * 4; }
		uint32_t GetTotalIndexCount() const { return m_QuadCounts * 6; }
	};

	static void ResetStats();
	static Statistics GetStats();

private:
	static void StartBatch();
	static void NextBatch();
};

_WHIP_END
