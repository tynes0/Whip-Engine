#pragma once

#include "RenderCommand.h"
#include "OrthographicCamera.h"
#include "Shader.h" 

_WHIP_START

struct SceneData
{
	glm::mat4 m_ViewProjectionMatrix;
};

class Renderer
{
private:
	static Ref<SceneData> s_SceneData;
public:
	static void Init();
	static void Shutdown();
	static void OnWindowResize(uint32_t width, uint32_t height);
	static void BeginScene(OrthographicCamera& camera);
	static void EndScene();
	static void Submit(const Ref<Shader>& shader, const Ref<VertexArray>& vertexArray, const glm::mat4& transform = glm::mat4(1.0f));
	WHP_NODISCARD inline static RenderAPI::API GetAPI() { return RenderAPI::GetAPI(); }
};

_WHIP_END
