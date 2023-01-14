#pragma once

#include <Whip/Core.h>
#include <Whip/Events/Event.h>
#include <Whip/Window.h>
#include <Whip/Events/ApplicationEvent.h>
#include <Whip/LayerStack.h>


_WHIP_START

class WHIP_API Application
{
private:
	static Application* s_Instance;
	std::unique_ptr<Window> m_Window;
	bool m_Running = true;
	LayerStack m_LayerStack;

	bool OnWindowClose(WindowCloseEvent& event);
public:
	Application();
	virtual ~Application();


	void Run();
	void OnEvent(Event& e);
	void PushLayer(layerptr layer);
	void PushOverlay(layerptr overlay);

	inline static Application& Get() { return *s_Instance; }
	inline Window& GetWindow() { return *m_Window; }
};

// to be defined in client
Application* CreateApplication();

_WHIP_END