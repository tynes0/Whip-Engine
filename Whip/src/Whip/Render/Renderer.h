#pragma once

#include "RenderCommand.h"
#include "orthographic_camera.h"
#include "Shader.h" 

_WHIP_START

struct scene_data
{
	glm::mat4 view_projection_matrix;
};

class renderer
{
private:
	static ref<scene_data> m_scene_data;
public:
	static void init();
	static void shutdown();
	static void on_window_resize(uint32_t width, uint32_t height);
	static void begin_scene(orthographic_camera& camera);
	static void end_scene();
	static void submit(const ref<shader>& shader, const ref<vertex_array>& vertexArray, const glm::mat4& transform = glm::mat4(1.0f));
	WHP_NODISCARD inline static render_API::API get_API() { return render_API::get_API(); }
};

_WHIP_END