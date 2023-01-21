#pragma once

#include <Whip/Window.h>
#include <Whip/Events/ApplicationEvent.h>
#include <Whip/LayerStack.h>
#include <Whip/ImGui/ImGuiLayer.h>
#include <Whip/Render/Shader.h>
#include <Whip/Render/Buffer.h>
#include <Whip/Render/VertexArray.h>

_WHIP_START

class WHIP_API Application
{
private:
	static Application* s_Instance;
	std::unique_ptr<Window> m_Window;
	std::shared_ptr<Shader> m_Shader;
	std::shared_ptr<VertexArray> m_VertexArray;
	ImGuiLayer* m_ImGuiLayer;
	LayerStack m_LayerStack;
	bool m_Running = true;

	std::shared_ptr<Shader> m_SquareShader;
	std::shared_ptr<VertexArray> m_SquareVertexArray;
private:
	bool OnWindowClose(WindowCloseEvent& event);
public:
	Application();
	virtual ~Application();


	void Run();
	void OnEvent(Event& e);
	void PushLayer(layerptr layer);
	void PushOverlay(layerptr overlay);

	WHP_NODISCARD inline static Application& Get() { return DREF(s_Instance); }
	WHP_NODISCARD inline Window& GetWindow() { return DREF(m_Window); }
};

// to be defined in client
Application* CreateApplication();

_WHIP_END