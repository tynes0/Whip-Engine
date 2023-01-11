#include <whippch.h>
#include <Whip/Application.h>

_WHIP_START

#define BIND_APP_EVENT_FN(x) std::bind(&Application::x, this, std::placeholders::_1)

Application::Application()
{
	m_Window = std::unique_ptr<Window>(Window::Create());
	m_Window->SetEventCallback(BIND_APP_EVENT_FN(OnEvent));
}


Application::~Application()
{

}

void Application::Run()
{
	while (m_Running)
	{
		for (layerptr item : m_LayerStack)
		{
			item->OnUpdate();
		}
		m_Window->OnUpdate();
	}
}

void Application::OnEvent(Event& e)
{
	EventDispatcher dispatcher(e);
	dispatcher.Dispatch<WindowCloseEvent>(BIND_APP_EVENT_FN(OnWindowClose));
	WHP_CORE_TRACE("{0}", e);

	for (auto iter = m_LayerStack.end(); iter != m_LayerStack.begin(); )
	{
		(*--iter)->OnEvent(e);
		if (e.Handled)
		{
			break;
		}
	}
}

void Application::PushLayer(layerptr layer)
{
	m_LayerStack.PushLayer(layer);
}

void Application::PushOverlay(layerptr overlay)
{
	m_LayerStack.PushOverlay(overlay);
}

bool Application::OnWindowClose(WindowCloseEvent& event)
{
	m_Running = false;
	return true;
}

_WHIP_END