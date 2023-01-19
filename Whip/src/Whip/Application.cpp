#include <whippch.h>
#include <Whip/Application.h>
#include <Whip/Input.h>

_WHIP_START

Application* Application::s_Instance = nullptr;

Application::Application()
{
	WHP_CORE_ASSERT(!s_Instance, "Application already exist!");
	s_Instance = this;
	m_Window = std::unique_ptr<Window>(Window::Create());
	m_Window->SetEventCallback(WHP_BIND_EVENT_FN(Application::OnEvent));
	m_ImGuiLayer = new ImGuiLayer();
	PushOverlay(m_ImGuiLayer);
}


Application::~Application()
{

}

void Application::Run()
{
	while (m_Running)
	{
		m_ImGuiLayer->begin();
		for (layerptr item : m_LayerStack)
		{
			item->OnImGuiRender();
		}
		m_ImGuiLayer->end();
		m_Window->OnUpdate();
	}
}

void Application::OnEvent(Event& e)
{
	EventDispatcher dispatcher(e);
	dispatcher.Dispatch<WindowCloseEvent>(WHP_BIND_EVENT_FN(Application::OnWindowClose));
	WHP_CORE_DEBUGL("{0}", e);

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
	layer->OnAttach();
}

void Application::PushOverlay(layerptr overlay)
{
	m_LayerStack.PushOverlay(overlay);
	overlay->OnAttach();
}

bool Application::OnWindowClose(WindowCloseEvent& event)
{
	m_Running = false;
	return true;
}

_WHIP_END