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
};

// to be defined in client
Application* CreateApplication();

_WHIP_END