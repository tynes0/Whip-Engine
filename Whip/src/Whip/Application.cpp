#include "whippch.h"
#include "Application.h"

#include "Whip/Log.h"
#include "Whip/Events/ApplicationEvent.h"

_WHIP_START

Application::Application()
{
	m_Window = std::unique_ptr<Window>(Window::Create());
}


Application::~Application()
{

}

void Application::Run()
{
	while (m_Running)
	{
		m_Window->OnUpdate();
	}
}

_WHIP_END