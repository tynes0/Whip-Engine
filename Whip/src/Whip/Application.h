#pragma once
#include "Core.h"
#include "Events/Event.h"
#include "Window.h"


_WHIP_START

class WHIP_API Application
{
private:
	std::unique_ptr<Window> m_Window;
	bool m_Running = true;
public:
	Application();
	virtual ~Application();
	void Run();
};

// to be defined in client
Application* CreateApplication();

_WHIP_END