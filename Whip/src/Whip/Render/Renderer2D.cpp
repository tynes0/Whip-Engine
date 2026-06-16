#include "WhipPch.h"
#include <Whip/Render/Renderer2D.h>

#include <Whip/Render/VertexArray.h>
#include <Whip/Render/Shader.h>
#include <Whip/Render/RenderCommand.h>
#include <Whip/Render/UniformBuffer.h>
#include <Whip/Render/MsdfData.h>

#include <Whip/Asset/AssetManager.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <array>

_WHIP_START


struct QuadVertex
{
	glm::vec3 m_Position;
	glm::vec4 m_Color;
	glm::vec2 m_TextureCoord;
	float m_TextureIndex;
	float m_TilingFactor;
	int m_EntityId; // editor only
};

struct CircleVertex
{
	glm::vec3 m_WorldPosition;
	glm::vec3 m_LocalPosition;
	glm::vec4 m_Color;
	float m_Thickness;
	float m_Fade;
	int m_EntityId; // editor only
};

struct LineVertex
{
	glm::vec3 m_Position;
	glm::vec4 m_Color;
	int m_EntityId; // editor only
};

struct TextVertex
{
	glm::vec3 m_Position;
	glm::vec4 m_Color;
	glm::vec2 m_TextureCoord;
	// TODO: bg color for outline/bg
	int m_EntityId; // editor only
};

struct Renderer2DData
{
	static constexpr uint32_t MaxQuads = 20000;
	static constexpr uint32_t MaxVertices = MaxQuads * 4;
	static constexpr uint32_t MaxIndices = MaxQuads * 6;
	static constexpr uint32_t MaxTextureSlots = 32; // TODO: render_caps

	Ref<VertexArray> m_QuadVertexArray;
	Ref<VertexBuffer> m_QuadVertexBuffer;
	Ref<Shader> m_QuadShader;
	Ref<Texture2D> m_WhiteTexture;

	Ref<VertexArray> m_CircleVertexArray;
	Ref<VertexBuffer> m_CircleVertexBuffer;
	Ref<Shader> m_CircleShader;

	Ref<VertexArray> m_LineVertexArray;
	Ref<VertexBuffer> m_LineVertexBuffer;
	Ref<Shader> m_LineShader;

	Ref<VertexArray> m_TextVertexArray;
	Ref<VertexBuffer> m_TextVertexBuffer;
	Ref<Shader> m_TextShader;

	uint32_t m_QuadIndexCount = 0;
	QuadVertex* m_QuadVertexBufferBase = nullptr;
	QuadVertex* m_QuadVertexBufferPtr = nullptr;

	uint32_t m_CircleIndexCount = 0;
	CircleVertex* m_CircleVertexBufferBase = nullptr;
	CircleVertex* m_CircleVertexBufferPtr = nullptr;

	uint32_t m_LineVertexCount = 0;
	LineVertex* m_LineVertexBufferBase = nullptr;
	LineVertex* m_LineVertexBufferPtr = nullptr;

	uint32_t m_TextIndexCount = 0;
	TextVertex* m_TextVertexBufferBase = nullptr;
	TextVertex* m_TextVertexBufferPtr = nullptr;

	std::array <Ref<Texture2D>, MaxTextureSlots> m_TextureSlots;
	uint32_t m_TextureSlotIndex = 1; // 0 = white Texture

	Ref<Texture2D> m_FontAtlasTexture;

	glm::vec4 m_QuadVertexPositions[4] = {};

	Renderer2D::Statistics m_Stats;

	struct CameraData
	{
		glm::mat4 m_ViewProjection;
	};
	CameraData m_CameraBuffer{};
	Ref<UniformBuffer> m_CameraUniformBuffer;
};


namespace
{
	Renderer2DData s_Data;

	template <typename T>
	void ReleaseVertexBuffer(T*& buffer)
	{
		delete[] buffer;
		buffer = nullptr;
	}

	void SetAndIncrementQuadVertexBufferPtr(const glm::vec3& position, const glm::vec4& color, glm::vec2 textureCoord, float textureIndex, float tilingFactor, int entityId)
	{
		s_Data.m_QuadVertexBufferPtr->m_Position = position;
		s_Data.m_QuadVertexBufferPtr->m_Color = color;
		s_Data.m_QuadVertexBufferPtr->m_TextureCoord = textureCoord;
		s_Data.m_QuadVertexBufferPtr->m_TextureIndex = textureIndex;
		s_Data.m_QuadVertexBufferPtr->m_TilingFactor = tilingFactor;
		s_Data.m_QuadVertexBufferPtr->m_EntityId = entityId;
		s_Data.m_QuadVertexBufferPtr++;
	}
}

void Renderer2D::Init()
{
	WHP_PROFILE_FUNCTION();

	s_Data.m_QuadVertexArray = VertexArray::Create();
	s_Data.m_QuadVertexBuffer = VertexBuffer::Create(s_Data.MaxVertices * sizeof(QuadVertex));
	s_Data.m_QuadVertexBuffer->SetLayout({
			{whip::ShaderDataType::Float3, "a_position"},
			{whip::ShaderDataType::Float4, "a_color"},
			{whip::ShaderDataType::Float2, "a_texture_coord"},
			{whip::ShaderDataType::Float,  "a_texture_index"},
			{whip::ShaderDataType::Float,  "a_tiling_factor"},
			{whip::ShaderDataType::Int,  "a_entityID"}
		});
	s_Data.m_QuadVertexArray->AddVertexBuffer(s_Data.m_QuadVertexBuffer);

	s_Data.m_QuadVertexBufferBase = new QuadVertex[s_Data.MaxVertices];

	uint32_t* quadIndices = new uint32_t[s_Data.MaxIndices];

	uint32_t offset = 0;

	_WHP_PRAGMA_WARNING(push)
		_WHP_PRAGMA_WARNING_DISABLE(6386)

		for (uint32_t i = 0; i < s_Data.MaxIndices; i += 6)
		{
			quadIndices[i + 0] = offset + 0;
			quadIndices[i + 1] = offset + 1;
			quadIndices[i + 2] = offset + 2;
			quadIndices[i + 3] = offset + 2;
			quadIndices[i + 4] = offset + 3;
			quadIndices[i + 5] = offset + 0;

			offset += 4;
		}

	_WHP_PRAGMA_WARNING(pop)

	Ref<IndexBuffer> quadIndexBuffer = IndexBuffer::Create(quadIndices, s_Data.MaxIndices);
	s_Data.m_QuadVertexArray->SetIndexBuffer(quadIndexBuffer);
	delete[] quadIndices;

	// Circles
	s_Data.m_CircleVertexArray = VertexArray::Create();

	s_Data.m_CircleVertexBuffer = VertexBuffer::Create(s_Data.MaxVertices * sizeof(CircleVertex));
	s_Data.m_CircleVertexBuffer->SetLayout({
		{ ShaderDataType::Float3, "a_world_position" },
		{ ShaderDataType::Float3, "a_local_position" },
		{ ShaderDataType::Float4, "a_color"         },
		{ ShaderDataType::Float,  "a_thickness"     },
		{ ShaderDataType::Float,  "a_fade"          },
		{ ShaderDataType::Int,    "a_entityID"      }
		});
	s_Data.m_CircleVertexArray->AddVertexBuffer(s_Data.m_CircleVertexBuffer);
	s_Data.m_CircleVertexArray->SetIndexBuffer(quadIndexBuffer);
	s_Data.m_CircleVertexBufferBase = new CircleVertex[s_Data.MaxVertices];

	// lines
	s_Data.m_LineVertexArray = VertexArray::Create();

	s_Data.m_LineVertexBuffer = VertexBuffer::Create(s_Data.MaxVertices * sizeof(LineVertex));
	s_Data.m_LineVertexBuffer->SetLayout({
		{ ShaderDataType::Float3, "a_position" },
		{ ShaderDataType::Float4, "a_color"    },
		{ ShaderDataType::Int,    "a_entityID" }
		});
	s_Data.m_LineVertexArray->AddVertexBuffer(s_Data.m_LineVertexBuffer);
	s_Data.m_LineVertexBufferBase = new LineVertex[s_Data.MaxVertices];

	// texts
	s_Data.m_TextVertexArray = VertexArray::Create();

	s_Data.m_TextVertexBuffer = VertexBuffer::Create(s_Data.MaxVertices * sizeof(TextVertex));
	s_Data.m_TextVertexBuffer->SetLayout({
		{ ShaderDataType::Float3, "a_position"     },
		{ ShaderDataType::Float4, "a_color"        },
		{ ShaderDataType::Float2, "a_texture_coord"},
		{ ShaderDataType::Int,    "a_entityID"     }
		});
	s_Data.m_TextVertexArray->AddVertexBuffer(s_Data.m_TextVertexBuffer);
	s_Data.m_TextVertexArray->SetIndexBuffer(quadIndexBuffer);
	s_Data.m_TextVertexBufferBase = new TextVertex[s_Data.MaxVertices];

	s_Data.m_WhiteTexture = Texture2D::Create(TextureSpecification{});
	uint32_t whiteTextureData = 0xffffffff;
	s_Data.m_WhiteTexture->SetData(RawBuffer(&whiteTextureData, sizeof(whiteTextureData)));

	s_Data.m_QuadShader		= Shader::Create("assets\\shaders\\renderer2D_quad.glsl");
	s_Data.m_CircleShader	= Shader::Create("assets\\shaders\\renderer2D_circle.glsl");
	s_Data.m_LineShader		= Shader::Create("assets\\shaders\\renderer2D_line.glsl");
	s_Data.m_TextShader		= Shader::Create("assets\\shaders\\renderer2D_text.glsl");

	s_Data.m_TextureSlots[0] = s_Data.m_WhiteTexture;

	s_Data.m_QuadVertexPositions[0] = { -0.5f, -0.5f, 0.0f, 1.0f };
	s_Data.m_QuadVertexPositions[1] = {  0.5f, -0.5f, 0.0f, 1.0f };
	s_Data.m_QuadVertexPositions[2] = {  0.5f,	0.5f, 0.0f, 1.0f };
	s_Data.m_QuadVertexPositions[3] = { -0.5f,  0.5f, 0.0f, 1.0f };

	s_Data.m_CameraUniformBuffer = UniformBuffer::Create(sizeof(Renderer2DData::CameraData), 0);

	SetLineWidth(2);
}

void Renderer2D::Shutdown()
{
	WHP_PROFILE_FUNCTION();

	ReleaseVertexBuffer(s_Data.m_QuadVertexBufferBase);
	ReleaseVertexBuffer(s_Data.m_CircleVertexBufferBase);
	ReleaseVertexBuffer(s_Data.m_LineVertexBufferBase);
	ReleaseVertexBuffer(s_Data.m_TextVertexBufferBase);

	s_Data.m_QuadVertexBufferPtr = nullptr;
	s_Data.m_CircleVertexBufferPtr = nullptr;
	s_Data.m_LineVertexBufferPtr = nullptr;
	s_Data.m_TextVertexBufferPtr = nullptr;

	for (Ref<Texture2D>& textureSlot : s_Data.m_TextureSlots)
		textureSlot.reset();

	s_Data.m_FontAtlasTexture.reset();
	s_Data.m_WhiteTexture.reset();

	s_Data.m_QuadShader.reset();
	s_Data.m_CircleShader.reset();
	s_Data.m_LineShader.reset();
	s_Data.m_TextShader.reset();

	s_Data.m_QuadVertexBuffer.reset();
	s_Data.m_CircleVertexBuffer.reset();
	s_Data.m_LineVertexBuffer.reset();
	s_Data.m_TextVertexBuffer.reset();

	s_Data.m_QuadVertexArray.reset();
	s_Data.m_CircleVertexArray.reset();
	s_Data.m_LineVertexArray.reset();
	s_Data.m_TextVertexArray.reset();

	s_Data.m_CameraUniformBuffer.reset();

	s_Data.m_QuadIndexCount = 0;
	s_Data.m_CircleIndexCount = 0;
	s_Data.m_LineVertexCount = 0;
	s_Data.m_TextIndexCount = 0;
	s_Data.m_TextureSlotIndex = 1;
}

void Renderer2D::BeginScene(const OrthographicCamera& camera)
{
	WHP_PROFILE_FUNCTION();

	s_Data.m_QuadShader->Bind();
	s_Data.m_QuadShader->SetMat4("u_view_projection", camera.GetViewProjectionMatrix());

	StartBatch();
}

void Renderer2D::BeginScene(const Camera& cam, const glm::mat4& transform)
{
	WHP_PROFILE_FUNCTION();

	s_Data.m_CameraBuffer.m_ViewProjection = cam.GetProjection() * glm::inverse(transform);
	s_Data.m_CameraUniformBuffer->SetData(&s_Data.m_CameraBuffer, sizeof(Renderer2DData::CameraData));

	StartBatch();
}

void Renderer2D::BeginScene(const EditorCamera& cam)
{
	WHP_PROFILE_FUNCTION();

	s_Data.m_CameraBuffer.m_ViewProjection = cam.GetViewProjection();
	s_Data.m_CameraUniformBuffer->SetData(&s_Data.m_CameraBuffer, sizeof(Renderer2DData::CameraData));

	StartBatch();
}

void Renderer2D::EndScene()
{
	WHP_PROFILE_FUNCTION();
	Flush();
}

void Renderer2D::Flush()
{
	if (s_Data.m_QuadIndexCount)
	{
		uint32_t dataSize = static_cast<uint32_t>(reinterpret_cast<uint8_t*>(s_Data.m_QuadVertexBufferPtr) - reinterpret_cast<uint8_t*>(s_Data.m_QuadVertexBufferBase));
		s_Data.m_QuadVertexBuffer->SetData(s_Data.m_QuadVertexBufferBase, dataSize);
		// Bind textures
		for (uint32_t i = 0; i < s_Data.m_TextureSlotIndex; i++)
			s_Data.m_TextureSlots[i]->Bind(i);

		s_Data.m_QuadShader->Bind();
		RenderCommand::DrawIndexed(s_Data.m_QuadVertexArray, s_Data.m_QuadIndexCount);
	s_Data.m_Stats.m_DrawCalls++;
	}

	if (s_Data.m_CircleIndexCount)
	{
		uint32_t dataSize = static_cast<uint32_t>(reinterpret_cast<uint8_t*>(s_Data.m_CircleVertexBufferPtr) - reinterpret_cast<uint8_t*>(s_Data.m_CircleVertexBufferBase));
		s_Data.m_CircleVertexBuffer->SetData(s_Data.m_CircleVertexBufferBase, dataSize);

		s_Data.m_CircleShader->Bind();
		RenderCommand::DrawIndexed(s_Data.m_CircleVertexArray, s_Data.m_CircleIndexCount);
	s_Data.m_Stats.m_DrawCalls++;
	}

	if (s_Data.m_LineVertexCount)
	{
		uint32_t dataSize = static_cast<uint32_t>(reinterpret_cast<uint8_t*>(s_Data.m_LineVertexBufferPtr) - reinterpret_cast<uint8_t*>(s_Data.m_LineVertexBufferBase));
		s_Data.m_LineVertexBuffer->SetData(s_Data.m_LineVertexBufferBase, dataSize);

		s_Data.m_LineShader->Bind();
		RenderCommand::DrawLines(s_Data.m_LineVertexArray, s_Data.m_LineVertexCount);
	s_Data.m_Stats.m_DrawCalls++;
	}

	if (s_Data.m_TextIndexCount)
	{
		uint32_t dataSize = static_cast<uint32_t>(reinterpret_cast<uint8_t*>(s_Data.m_TextVertexBufferPtr) - reinterpret_cast<uint8_t*>(s_Data.m_TextVertexBufferBase));
		s_Data.m_TextVertexBuffer->SetData(s_Data.m_TextVertexBufferBase, dataSize);

		s_Data.m_FontAtlasTexture->Bind(0);

		s_Data.m_TextShader->Bind();
		RenderCommand::DrawIndexed(s_Data.m_TextVertexArray, s_Data.m_TextIndexCount);
	s_Data.m_Stats.m_DrawCalls++;
	}
}


void Renderer2D::DrawQuad(const glm::mat4& transform, const glm::vec4& color, int entityId)
{
	WHP_PROFILE_FUNCTION();

	if (s_Data.m_QuadIndexCount >= Renderer2DData::MaxIndices)
		NextBatch();

	constexpr size_t QuadVertexCount = 4u;
	constexpr float textureIndex = 0.0f; // white color
	constexpr float tilingFactor = 1.0f;
	constexpr glm::vec2 textureCoords[] = { {0.0f,0.0f},{1.0f,0.0f},{1.0f,1.0f},{0.0f,1.0f} };

	for (size_t i = 0; i < QuadVertexCount; ++i)
		SetAndIncrementQuadVertexBufferPtr(transform * s_Data.m_QuadVertexPositions[i], color, textureCoords[i], textureIndex, tilingFactor, entityId);
	s_Data.m_QuadIndexCount += 6;
	s_Data.m_Stats.m_QuadCounts++;
}

void Renderer2D::DrawQuad(const glm::mat4& transform, const Ref<Texture2D>& tex, float tilingFactor, const glm::vec4& tintColor, int entityId)
{
	WHP_PROFILE_FUNCTION();
	WHP_CORE_VERIFY(tex)

	if (s_Data.m_QuadIndexCount >= Renderer2DData::MaxIndices)
		NextBatch();

	constexpr size_t QuadVertexCount = 4u;
	constexpr glm::vec2 textureCoords[] = { {0.0f,0.0f},{1.0f,0.0f},{1.0f,1.0f},{0.0f,1.0f} };

	float textureIndex = 0.0f;

	for (uint32_t i = 1; i < s_Data.m_TextureSlotIndex; ++i)
		if (DREF(s_Data.m_TextureSlots[i]) == DREF(tex))
		{
			textureIndex = static_cast<float>(i);
			break;
		}

_WHP_PRAGMA_WARNING(push)
_WHP_PRAGMA_WARNING_DISABLE(28020)
	if (textureIndex == 0.0f)
	{
		if (s_Data.m_TextureSlotIndex >= Renderer2DData::MaxTextureSlots)
			NextBatch();
		textureIndex = static_cast<float>(s_Data.m_TextureSlotIndex);
		s_Data.m_TextureSlots[s_Data.m_TextureSlotIndex] = tex;
		s_Data.m_TextureSlotIndex++;
	}
_WHP_PRAGMA_WARNING(pop)

	for (size_t i = 0; i < QuadVertexCount; ++i)
		SetAndIncrementQuadVertexBufferPtr(transform * s_Data.m_QuadVertexPositions[i], tintColor, textureCoords[i], textureIndex, tilingFactor, entityId);

	s_Data.m_QuadIndexCount += 6;
	s_Data.m_Stats.m_QuadCounts++;
}

void Renderer2D::DrawQuad(const glm::mat4& transform, const Ref<SubTexture2D>& subTexture, float tilingFactor, const glm::vec4& tintColor, int entityId)
{
	WHP_PROFILE_FUNCTION();

	if (s_Data.m_QuadIndexCount >= Renderer2DData::MaxIndices)
		NextBatch();

	constexpr size_t QuadVertexCount = 4u;
	const glm::vec2* textureCoords = subTexture->GetTextureCoords();
	auto tex = subTexture->GetTexture();

	float textureIndex = 0.0f;

	for (uint32_t i = 1; i < s_Data.m_TextureSlotIndex; ++i)
		if (DREF(s_Data.m_TextureSlots[i].get()) == DREF(tex.get()))
			textureIndex = static_cast<float>(i);

	if (textureIndex == 0.0f)
	{
		if (s_Data.m_TextureSlotIndex >= Renderer2DData::MaxTextureSlots)
			NextBatch();
		textureIndex = static_cast<float>(s_Data.m_TextureSlotIndex);
		s_Data.m_TextureSlots[s_Data.m_TextureSlotIndex++] = tex;
	}

	for (size_t i = 0; i < QuadVertexCount; ++i)
		SetAndIncrementQuadVertexBufferPtr(transform * s_Data.m_QuadVertexPositions[i], tintColor, textureCoords[i], textureIndex, tilingFactor, entityId);

	s_Data.m_QuadIndexCount += 6;
	s_Data.m_Stats.m_QuadCounts++;
}


void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color)
{
	DrawQuad({ position.x, position.y, 0.0f }, size, color);
}

void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color)
{
	glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
		* glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });
	DrawQuad(transform, color);
}

void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const Ref<Texture2D>& tex, float tilingFactor, const glm::vec4& tintColor)
{
	DrawQuad({ position.x, position.y, 0.0f }, size, tex, tilingFactor, tintColor);
}

void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const Ref<Texture2D>& tex, float tilingFactor, const glm::vec4& tintColor)
{
	glm::mat4 transform = glm::translate(glm::mat4(1.0f), position) * glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });
	DrawQuad(transform, tex, tilingFactor, tintColor);
}

void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const Ref<SubTexture2D>& subTexture, float tilingFactor, const glm::vec4& tintColor)
{
	DrawQuad({ position.x, position.y, 0.0f }, size, subTexture, tilingFactor, tintColor);
}

void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const Ref<SubTexture2D>& subTexture, float tilingFactor, const glm::vec4& tintColor)
{
	glm::mat4 transform = glm::translate(glm::mat4(1.0f), position) * glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });
	DrawQuad(transform, subTexture, tilingFactor, tintColor);
}

void Renderer2D::DrawRotatedQuad(const glm::vec2& position, const glm::vec2& size, float rotation, const glm::vec4& color)
{
	DrawRotatedQuad({ position.x, position.y, 0.0f }, size, rotation, color);
}

void Renderer2D::DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float rotation, const glm::vec4& color)
{
	glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
		* glm::rotate(glm::mat4(1.0f), rotation, { 0.0f, 0.0f, 1.0f })
		* glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });
	DrawQuad(transform, color);
}

void Renderer2D::DrawRotatedQuad(const glm::vec2& position, const glm::vec2& size, float rotation, const Ref<Texture2D>& tex, float tilingFactor, const glm::vec4& tintColor)
{
	DrawRotatedQuad({ position.x, position.y, 0.0f }, size, rotation, tex, tilingFactor, tintColor);
}

void Renderer2D::DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float rotation, const Ref<Texture2D>& tex, float tilingFactor, const glm::vec4& tintColor)
{
	glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
		* glm::rotate(glm::mat4(1.0f), rotation, { 0.0f, 0.0f, 1.0f })
		* glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

	DrawQuad(transform, tex, tilingFactor, tintColor);
}

void Renderer2D::DrawRotatedQuad(const glm::vec2& position, const glm::vec2& size, float rotation, const Ref<SubTexture2D>& subTexture, float tilingFactor, const glm::vec4& tintColor)
{
	DrawRotatedQuad({ position.x, position.y, 0.0f }, size, rotation, subTexture, tilingFactor, tintColor);
}

void Renderer2D::DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float rotation, const Ref<SubTexture2D>& subTexture, float tilingFactor, const glm::vec4& tintColor)
{
	glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
		* glm::rotate(glm::mat4(1.0f), rotation, { 0.0f, 0.0f, 1.0f })
		* glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

	DrawQuad(transform, subTexture, tilingFactor, tintColor);
}

void Renderer2D::DrawCircle(const glm::mat4& transform, const glm::vec4& color, float thickness, float fade, int entityId)
{
	WHP_PROFILE_FUNCTION();

	for (auto quadVertexPosition : s_Data.m_QuadVertexPositions)
	{
		s_Data.m_CircleVertexBufferPtr->m_WorldPosition = transform * quadVertexPosition;
		s_Data.m_CircleVertexBufferPtr->m_LocalPosition = quadVertexPosition * 2.0f;
		s_Data.m_CircleVertexBufferPtr->m_Color = color;
		s_Data.m_CircleVertexBufferPtr->m_Thickness = thickness;
		s_Data.m_CircleVertexBufferPtr->m_Fade = fade;
		s_Data.m_CircleVertexBufferPtr->m_EntityId = entityId;
		s_Data.m_CircleVertexBufferPtr++;
	}

	s_Data.m_CircleIndexCount += 6;

	s_Data.m_Stats.m_QuadCounts++;
}

void Renderer2D::DrawLine(const glm::vec3& p0, const glm::vec3& p1, const glm::vec4& color, int entityId)
{
	s_Data.m_LineVertexBufferPtr->m_Position = p0;
	s_Data.m_LineVertexBufferPtr->m_Color = color;
	s_Data.m_LineVertexBufferPtr->m_EntityId = entityId;
	s_Data.m_LineVertexBufferPtr++;

	s_Data.m_LineVertexBufferPtr->m_Position = p1;
	s_Data.m_LineVertexBufferPtr->m_Color = color;
	s_Data.m_LineVertexBufferPtr->m_EntityId = entityId;
	s_Data.m_LineVertexBufferPtr++;

	s_Data.m_LineVertexCount += 2;
}

void Renderer2D::DrawRect(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color, int entityId)
{
	glm::vec3 p0 = glm::vec3(position.x - size.x * 0.5f, position.y - size.y * 0.5f, position.z);
	glm::vec3 p1 = glm::vec3(position.x + size.x * 0.5f, position.y - size.y * 0.5f, position.z);
	glm::vec3 p2 = glm::vec3(position.x + size.x * 0.5f, position.y + size.y * 0.5f, position.z);
	glm::vec3 p3 = glm::vec3(position.x - size.x * 0.5f, position.y + size.y * 0.5f, position.z);

	DrawLine(p0, p1, color, entityId);
	DrawLine(p1, p2, color, entityId);
	DrawLine(p2, p3, color, entityId);
	DrawLine(p3, p0, color, entityId);
}

void Renderer2D::DrawRect(const glm::mat4& transform, const glm::vec4& color, int entityId)
{
	glm::vec3 lineVertices[4];
	for (size_t i = 0; i < 4; i++)
		lineVertices[i] = transform * s_Data.m_QuadVertexPositions[i];

	DrawLine(lineVertices[0], lineVertices[1], color, entityId);
	DrawLine(lineVertices[1], lineVertices[2], color, entityId);
	DrawLine(lineVertices[2], lineVertices[3], color, entityId);
	DrawLine(lineVertices[3], lineVertices[0], color, entityId);
}

void Renderer2D::DrawSprite(const glm::mat4& transform, const SpriteRendererComponent& src, int entityId)
{
	if (src.m_Texture)
	{
		Ref<Texture2D> texture = AssetManager::GetAsset<Texture2D>(src.m_Texture);
		DrawQuad(transform, texture, src.m_TilingFactor, src.m_Color, entityId);
	}
	else
	{
		DrawQuad(transform, src.m_Color, entityId);
	}
}

void Renderer2D::DrawString(const std::string& text, Ref<Font> font, const glm::mat4& transform, const TextParams& params, int entityId)
{
	WHP_PROFILE_FUNCTION();
	if (!font)
	{
		WHP_CORE_ERROR("[Renderer2D] Null Font!");
		return;
	}
	const auto& fontGeometry = font->GetMsdfData()->m_FontGeometry;
	const auto& metrics = fontGeometry.getMetrics();
	Ref<Texture2D> fontAtlas = font->GetAtlasTexture();
	if (!fontAtlas)
	{
		WHP_CORE_ERROR("[Renderer2D] Font has no atlas Texture!");
		return;
	}

	if (s_Data.m_TextIndexCount && s_Data.m_FontAtlasTexture && s_Data.m_FontAtlasTexture != fontAtlas)
		NextBatch();
	s_Data.m_FontAtlasTexture = fontAtlas;

	double x = 0.0;
	double fsScale = 1.0 / (metrics.ascenderY - metrics.descenderY);
	double y = 0.0;

	const auto spaceGlyph = fontGeometry.getGlyph(' ');
	const float spaceGlyphAdvance = spaceGlyph ? static_cast<float>(spaceGlyph->getAdvance()) : 1.0f;

	for (size_t i = 0; i < text.size(); i++)
	{
		char character = text[i];
		if (character == '\r')
			continue;

		if (character == '\n')
		{
			x = 0;
			y -= fsScale * metrics.lineHeight + params.m_LineSpacing;
			continue;
		}

		if (character == ' ')
		{
			float advance = spaceGlyphAdvance;
			if (i < text.size() - 1)
			{
				char nextCharacter = text[i + 1];
				double glyphAdvance;
				fontGeometry.getAdvance(glyphAdvance, character, nextCharacter);
				advance = static_cast<float>(glyphAdvance);
			}

			x += fsScale * advance + params.m_Kerning;
			continue;
		}

		if (character == '\t')
		{
			// is this right?
			x += 4.0f * (fsScale * spaceGlyphAdvance + params.m_Kerning);
			continue;
		}
		auto glyph = fontGeometry.getGlyph(character);
		if (!glyph)
			glyph = fontGeometry.getGlyph('?');
		if (!glyph)
			continue;

		if (s_Data.m_TextIndexCount >= Renderer2DData::MaxIndices)
			NextBatch();

		double al, ab, ar, at;
		glyph->getQuadAtlasBounds(al, ab, ar, at);
		glm::vec2 textureCoordMin(static_cast<float>(al), static_cast<float>(ab));
		glm::vec2 textureCoordMax(static_cast<float>(ar), static_cast<float>(at));

		double pl, pb, pr, pt;
		glyph->getQuadPlaneBounds(pl, pb, pr, pt);
		glm::vec2 quadMin(static_cast<float>(pl), static_cast<float>(pb));
		glm::vec2 quadMax(static_cast<float>(pr), static_cast<float>(pt));

		quadMin *= fsScale;
		quadMax *= fsScale;
		quadMin += glm::vec2(x, y);
		quadMax += glm::vec2(x, y);

		float texelWidth = 1.0f / static_cast<float>(fontAtlas->GetWidth());
		float texelHeight = 1.0f / static_cast<float>(fontAtlas->GetHeight());
		textureCoordMin *= glm::vec2(texelWidth, texelHeight);
		textureCoordMax *= glm::vec2(texelWidth, texelHeight);

		s_Data.m_TextVertexBufferPtr->m_Position = transform * glm::vec4(quadMin, 0.0f, 1.0f);
		s_Data.m_TextVertexBufferPtr->m_Color = params.m_Color;
		s_Data.m_TextVertexBufferPtr->m_TextureCoord = textureCoordMin;
		s_Data.m_TextVertexBufferPtr->m_EntityId = entityId;
		s_Data.m_TextVertexBufferPtr++;

		s_Data.m_TextVertexBufferPtr->m_Position = transform * glm::vec4(quadMin.x, quadMax.y, 0.0f, 1.0f);
		s_Data.m_TextVertexBufferPtr->m_Color = params.m_Color;
		s_Data.m_TextVertexBufferPtr->m_TextureCoord = { textureCoordMin.x, textureCoordMax.y };
		s_Data.m_TextVertexBufferPtr->m_EntityId = entityId;
		s_Data.m_TextVertexBufferPtr++;

		s_Data.m_TextVertexBufferPtr->m_Position = transform * glm::vec4(quadMax, 0.0f, 1.0f);
		s_Data.m_TextVertexBufferPtr->m_Color = params.m_Color;
		s_Data.m_TextVertexBufferPtr->m_TextureCoord = textureCoordMax;
		s_Data.m_TextVertexBufferPtr->m_EntityId = entityId;
		s_Data.m_TextVertexBufferPtr++;

		s_Data.m_TextVertexBufferPtr->m_Position = transform * glm::vec4(quadMax.x, quadMin.y, 0.0f, 1.0f);
		s_Data.m_TextVertexBufferPtr->m_Color = params.m_Color;
		s_Data.m_TextVertexBufferPtr->m_TextureCoord = { textureCoordMax.x, textureCoordMin.y };
		s_Data.m_TextVertexBufferPtr->m_EntityId = entityId;
		s_Data.m_TextVertexBufferPtr++;

		s_Data.m_TextIndexCount += 6;
		s_Data.m_Stats.m_QuadCounts++;

		if (i < text.size() - 1)
		{
			double advance = glyph->getAdvance();
			char nextCharacter = text[i + 1];
			fontGeometry.getAdvance(advance, character, nextCharacter);

			x += fsScale * advance + params.m_Kerning;
		}
	}
}

void Renderer2D::DrawString(const std::string& text, const glm::mat4& transform, const TextComponent& component, int entityId)
{
	DrawString(text,
		component.m_Font ? std::static_pointer_cast<Font>(Project::GetActive()->GetAssetManager()->GetAsset(component.m_Font)) : Font::GetDefault(),
		transform,
		{
			.m_Color = component.m_Color,
			.m_Kerning = component.m_Kerning,
			.m_LineSpacing = component.m_LineSpacing
		},
		entityId);
}

void Renderer2D::SetLineWidth(float width)
{
	RenderCommand::SetLineWidth(width);
}

void Renderer2D::ResetStats()
{
	memset(&s_Data.m_Stats, 0, sizeof(Renderer2D::Statistics));
}

Renderer2D::Statistics Renderer2D::GetStats()
{
	return s_Data.m_Stats;
}

void Renderer2D::StartBatch()
{
	s_Data.m_QuadIndexCount = 0;
	s_Data.m_QuadVertexBufferPtr = s_Data.m_QuadVertexBufferBase;

	s_Data.m_CircleIndexCount = 0;
	s_Data.m_CircleVertexBufferPtr = s_Data.m_CircleVertexBufferBase;

	s_Data.m_LineVertexCount = 0;
	s_Data.m_LineVertexBufferPtr = s_Data.m_LineVertexBufferBase;

	s_Data.m_TextIndexCount = 0;
	s_Data.m_TextVertexBufferPtr = s_Data.m_TextVertexBufferBase;

	s_Data.m_TextureSlotIndex = 1;
}

void Renderer2D::NextBatch()
{
	Flush();
	StartBatch();
}

_WHIP_END
