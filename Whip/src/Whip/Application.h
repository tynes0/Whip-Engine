#pragma once
#include "Core.h"
#include "Events/Event.h"



_WHIP_START

class WHIP_API Application
{
public:
	Application();
	virtual ~Application();
	void Run();
};

// to be defined in client
Application* CreateApplication();

_WHIP_END