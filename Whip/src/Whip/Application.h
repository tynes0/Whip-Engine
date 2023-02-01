#pragma once

#include <Whip/Window.h>
#include <Whip/Events/ApplicationEvent.h>
#include <Whip/LayerStack.h>
#include <Whip/ImGui/ImGuiLayer.h>
#include <Whip/Core/Timestep.h>

_WHIP_START

class Application
{
private:
	static Application* s_Instance;
	scope<Window> m_Window;
	ImGuiLayer* m_ImGuiLayer;
	LayerStack m_LayerStack;
	bool m_Running = true;
	float m_last_frame_time = 0.0f;
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