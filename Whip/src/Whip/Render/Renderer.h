#pragma once

#include "RenderCommand.h"
#include "Camera.h"
#include "Shader.h" 

_WHIP_START

struct scene_data
{
	glm::mat4 view_projection_matrix;
};

class Renderer
{
private:
	static scene_data* m_scene_data;
public:
	static void begin_scene(orthographic_camera& camera);
	static void end_scene();
	static void submit(const std::shared_ptr<Shader>& shader, const std::shared_ptr<VertexArray>& vertexArray, const glm::mat4& transform = glm::mat4(1.0f));
	WHP_NODISCARD inline static RenderAPI::API GetAPI() { return RenderAPI::GetAPI(); }
};

_WHIP_END