#pragma once

#include <Whip/Render/VertexArray.h>

#include <glm/glm.hpp>

_WHIP_START

class RenderAPI
{
public:
	enum class API
	{
		None = 0,
		OpenGL = 1
		// for now 
	};
public:
	virtual ~RenderAPI() = default;

	virtual void Init() = 0;
	virtual void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) = 0;
	virtual void SetClearColor(const glm::vec4& color) = 0;
	virtual void Clear() = 0;
	virtual void SetDepthTest(bool enabled) = 0;
	virtual void DrawIndexed(const whip::Ref<VertexArray>& vertexArray, uint32_t indexCount = 0) = 0;
	virtual void DrawLines(const Ref<VertexArray>& vertexArray, uint32_t vertexCount) = 0;
	virtual void SetLineWidth(float width) = 0;
	WHP_NODISCARD inline static API GetAPI() { return s_API; }
private:
	static API s_API;
};

_WHIP_END
