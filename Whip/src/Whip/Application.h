#pragma once

#include <Whip/Window.h>
#include <Whip/Events/ApplicationEvent.h>
#include <Whip/LayerStack.h>
#include <Whip/ImGui/ImGuiLayer.h>
#include <Whip/Render/Shader.h>
#include <Whip/Render/Buffer.h>

_WHIP_START

class WHIP_API Application
{
private:
	static Application* s_Instance;
	std::unique_ptr<Window> m_Window;
	std::unique_ptr<Shader> m_Shader;
	std::unique_ptr<VertexBuffer> m_VertexBuffer;
	std::unique_ptr<IndexBuffer> m_IndexBuffer;
	ImGuiLayer* m_ImGuiLayer;
	LayerStack m_LayerStack;
	bool m_Running = true;

	unsigned int m_VertexArray;

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