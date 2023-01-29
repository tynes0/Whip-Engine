#include <whippch.h>
#include <Whip/Application.h>

#include <GLFW/glfw3.h>

_WHIP_START

Application* Application::s_Instance = nullptr;

Application::Application()
{
	WHP_CORE_ASSERT(!s_Instance, "Application already exist!");
	s_Instance = this;
	m_Window = std::unique_ptr<Window>(Window::create());
	m_Window->set_event_callback(WHP_BIND_EVENT_FN(Application::OnEvent));
	m_Window->set_event_callback(std::bind(&Application::OnEvent, this, std::placeholders::_1));
	m_ImGuiLayer = new ImGuiLayer();
	PushOverlay(m_ImGuiLayer);
}

Application::~Application()
{

}
void swapx(int& t, int& y)
{
	int& u = t;
	t = y;
	y = u;
}


void Application::Run()
{
	while (m_Running)
	{
		float time = (float)glfwGetTime();
		timestep ts = time - m_last_frame_time;
		m_last_frame_time = time;
		//WHP_CORE_DEBUG("Delta time = {0}s ({1}ms)", ts, ts.get_milliseconds());
		for (layerptr item : m_LayerStack)
		{
			item->OnUpdate(ts);
		}

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
	//WHP_CORE_DEBUG("{0}", e);

	for (auto iter = m_LayerStack.end(); iter != m_LayerStack.begin(); )
	{
		(DREF(--iter))->OnEvent(e);
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
	WHP_CORE_DEBUG("Window destroyed!");
	return true;
}

_WHIP_END