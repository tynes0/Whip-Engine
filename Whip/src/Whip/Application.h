#pragma once
#include "Core.h"
#include "Events/Event.h"
#include "Whip/Window.h"
#include "Whip/Events/ApplicationEvent.h"


_WHIP_START

class WHIP_API Application
{
private:
	std::unique_ptr<Window> m_Window;
	bool m_Running = true;

	bool OnWindowClose(WindowCloseEvent& event);

public:
	Application();
	virtual ~Application();


	void Run();
	void OnEvent(Event& e);
};

// to be defined in client
Application* CreateApplication();

_WHIP_END