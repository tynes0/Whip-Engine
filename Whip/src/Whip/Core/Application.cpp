#include <WhipPch.h>
#include <Whip/Core/Application.h>

#include <Whip/Render/Renderer.h>
#include <Whip/Utils/PlatformUtils.h>
#include <Whip/Scripting/ScriptEngine.h>
#include <Whip/Audio/AudioEngine.h>
#include <Whip/Helper/TimerManager.h>
#include <Whip/Project/Project.h>

_WHIP_START

Application* Application::s_Instance = nullptr;

Application::Application(ApplicationSpecification spec)
	: m_Specification(std::move(spec)),
	m_MainThreadId(std::this_thread::get_id()),
	m_MainThreadQueue(memory::StlAllocator<std::function<void()>>(memory::GetAllocator(memory::MemoryTag::Core), memory::MemoryTag::Core)),
	m_NextTickQueue(memory::StlAllocator<std::function<void()>>(memory::GetAllocator(memory::MemoryTag::Core), memory::MemoryTag::Core))
{
	WHP_PROFILE_FUNCTION();

	WHP_CORE_ASSERT(!s_Instance, "Application already exist!");
	s_Instance = this;

	if (!m_Specification.m_WorkingDirectory.empty())
		std::filesystem::current_path(m_Specification.m_WorkingDirectory);

	m_Window = Scope<Window>(Window::Create(m_Specification.m_Properties));
	m_Window->SetEventCallback([this]<typename... T>(T&&... args) -> decltype(auto) { return this->OnEvent(std::forward<T>(args)...); });

	m_Window->SetVsync(false);
	Renderer::Init();
	AudioEngine::Init();

	m_ImGuiLayer = MakeRawTagged<ImGuiLayer>(memory::MemoryTag::Editor);
	PushOverlay(m_ImGuiLayer);
}

Application::~Application()
{
	WHP_PROFILE_FUNCTION();
	WHP_CORE_INFO("[Application] Shutdown started.");

	for (LayerPtr item : m_LayerStack)
		item->OnDetach();
	m_LayerStack.Clear();

	ScriptEngine::Shutdown();
	Project::SetActive(nullptr);
	Renderer::Shutdown();
	AudioEngine::Shutdown();

	WHP_CORE_INFO("[Application] Shutdown complete.");
}

void Application::Run()
{
	WHP_PROFILE_FUNCTION();

	while (m_Running)
	{
		WHP_PROFILE_SCOPE("Run Loop 1 Tick");

		m_TickCount++;

		{
			WHP_PROFILE_SCOPE("Next Tick Queue");
			ExecuteNextTickQueue();
		}

		if (!m_Running)
			break;

		float time = Time::GetTime();
		Timestep ts = time - m_LastFrameTime;
		m_LastFrameTime = time;

		{
			WHP_PROFILE_SCOPE("Main Thread Queue");
			ExecuteMainThreadQueue();
		}

		if (!m_Running)
			break;
		TimerManager::Get().Tick(ts);

		{
			WHP_PROFILE_SCOPE("Update Frame");

			{
				WHP_PROFILE_SCOPE("Update Layers");
				if (!m_Minimized)
				{
					for (LayerPtr item : m_LayerStack)
					{
						item->OnUpdate(ts);
					}
				}
			}
			{
				WHP_PROFILE_SCOPE("Update ImGui");
				m_ImGuiLayer->Begin();
				{
					for (LayerPtr item : m_LayerStack)
					{
						item->OnImGuiRender();
					}
				}
				m_ImGuiLayer->End();
			}
			{
				WHP_PROFILE_SCOPE("Update Window");
				m_Window->OnUpdate();
			}
		}

	}
}

void Application::Close()
{
	m_Running = false;
}

void Application::Restart()
{
	if (m_Restarting)
		return;

	if (Utils::RestartProgram())
	{
		m_Restarting = true;
		Close();
	}
}

void Application::OnEvent(Event& event)
{
	WHP_PROFILE_FUNCTION();

	EventDispatcher dispatcher(event);
	dispatcher.Dispatch<WindowCloseEvent>([this]<typename... T>(T&&... args) -> decltype(auto) { return this->OnWindowClose(std::forward<T>(args)...); });
	dispatcher.Dispatch<WindowResizeEvent>([this]<typename... T>(T&&... args) -> decltype(auto) { return this->OnWindowResize(std::forward<T>(args)...); });

	for (auto iter = m_LayerStack.end(); iter != m_LayerStack.begin(); )
	{
		if (event.m_Handled)
			break;
		(DREF(--iter))->OnEvent(event);
	}
}

void Application::PushLayer(LayerPtr layer)
{
	WHP_PROFILE_FUNCTION();

	m_LayerStack.PushLayer(layer);
	layer->OnAttach();
}

void Application::PushOverlay(LayerPtr overlay)
{
	WHP_PROFILE_FUNCTION();

	m_LayerStack.PushOverlay(overlay);
	overlay->OnAttach();
}

Application& Application::Get()
{
	return DREF(s_Instance);
}

Window& Application::GetWindow()
{
	return DREF(m_Window);
}

const Window& Application::GetWindow() const
{
	return DREF(m_Window);
}

ImGuiLayer* Application::GetImGuiLayer()
{
	return m_ImGuiLayer;
}

const ImGuiLayer* Application::GetImGuiLayer() const
{
	return m_ImGuiLayer;
}

ApplicationSpecification Application::GetSpecification() const
{
	return m_Specification;
}

uint64_t Application::GetTickCount() const
{
	return m_TickCount;
}

std::thread::id Application::GetMainThreadId() const
{
	return m_MainThreadId;
}

bool Application::IsMainThread() const
{
	return std::this_thread::get_id() == m_MainThreadId;
}

void Application::SubmitToMainThread(const std::function<void()>& function)
{
	std::scoped_lock<std::mutex> lock(m_MainThreadQueueMutex);

	m_MainThreadQueue.emplace_back(function);
}

void Application::SubmitToNextTick(const std::function<void()>& function)
{
	m_NextTickQueue.emplace_back(function);
}

bool Application::OnWindowClose(WindowCloseEvent& event)
{
	this->Close();
	WHP_CORE_INFO("[Application] Window destroyed!");
	return true;
}

bool Application::OnWindowResize(WindowResizeEvent& event)
{
	WHP_PROFILE_FUNCTION();

	if (event.GetWidth() == 0 || event.GetHeight() == 0)
	{
		m_Minimized = true;
		return false;
	}
	m_Minimized = false;
	Renderer::OnWindowResize(event.GetWidth(), event.GetHeight());

	return false;
}

void Application::ExecuteMainThreadQueue()
{
	std::scoped_lock<std::mutex> lock(m_MainThreadQueueMutex);

	for (auto& func : m_MainThreadQueue)
		func();

	m_MainThreadQueue.clear();
}

void Application::ExecuteNextTickQueue()
{
	for (auto& func : m_NextTickQueue)
		func();
	m_NextTickQueue.clear();
}

_WHIP_END
