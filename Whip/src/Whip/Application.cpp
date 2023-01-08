#include "whippch.h"
#include "Application.h"

#include "Whip/Log.h"
#include "Whip/Events/ApplicationEvent.h"

_WHIP_START

Application::Application()
{

}


Application::~Application()
{

}

void Application::Run()
{
	WindowResizeEvent e(1280, 720);
	WHP_TRACE(e);
	while (true);
}

_WHIP_END