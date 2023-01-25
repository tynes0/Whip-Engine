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
private:
	static API s_API;
public:
	virtual void SetClearColor(const glm::vec4& color) = 0;
	virtual void Clear() = 0;
	virtual void DrawIndexed(const std::shared_ptr<VertexArray>& vertexArray) = 0;
	WHP_NODISCARD inline static API GetAPI() { return s_API; }
};

_WHIP_END