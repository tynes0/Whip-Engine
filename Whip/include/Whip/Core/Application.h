#pragma once

#include "Window.h"
#include "Log.h"
#include "Memory.h"
#include <Whip/Core/Memory/AllocatorRegistry.h>
#include <Whip/Events/ApplicationEvent.h>
#include <Whip/Core/LayerStack.h>
#include <Whip/ImGui/ImGuiLayer.h>

#include <functional>
#include <thread>
#include <mutex>

int main(int argc, char** argv);

_WHIP_START

enum class ApplicationMode : uint8_t { Editor = 0, Runtime };

struct ApplicationCommandLineArgs
{
	int m_Count = 0;
	char** m_Args = nullptr;

	const char* operator[](int index) const
	{
		WHP_CORE_ASSERT(index < m_Count, "[Application] arg index is out of the range!");
		return m_Args[index];
	}
};

struct ApplicationSpecification
{
	WindowProps m_Properties;
	std::string m_WorkingDirectory;
	ApplicationCommandLineArgs m_CommandLineArgs;
	ApplicationMode m_Mode = ApplicationMode::Runtime;
};

class Application
{
public:
	Application(ApplicationSpecification spec);
	virtual ~Application();

	void Close();
	void Restart();
	void OnEvent(Event& event);
	void PushLayer(LayerPtr layer);
	void PushOverlay(LayerPtr overlay);

	WHP_NODISCARD static Application& Get();
	WHP_NODISCARD Window& GetWindow();
	WHP_NODISCARD const Window& GetWindow() const;
	WHP_NODISCARD ImGuiLayer* GetImGuiLayer();
	WHP_NODISCARD const ImGuiLayer* GetImGuiLayer() const;
	WHP_NODISCARD ApplicationSpecification GetSpecification() const;
	WHP_NODISCARD ApplicationMode GetMode() const;
	WHP_NODISCARD uint64_t GetTickCount() const;
	WHP_NODISCARD std::thread::id GetMainThreadId() const;
	WHP_NODISCARD bool IsMainThread() const;

	void SubmitToMainThread(const std::function<void()>& function);
	void SubmitToNextTick(const std::function<void()>& function);
private:
	void Run();

	bool OnWindowClose(WindowCloseEvent& event);
	bool OnWindowResize(WindowResizeEvent& event);

	void ExecuteMainThreadQueue();
	void ExecuteNextTickQueue();
private:
	static Application* s_Instance;
private:
	ApplicationSpecification m_Specification;
	Scope<Window> m_Window;
	ImGuiLayer* m_ImGuiLayer;
	LayerStack m_LayerStack;
	bool m_Running = true;
	bool m_Restarting = false;
	bool m_Minimized = false;
	float m_LastFrameTime = 0.0f;
	uint64_t m_TickCount = 0;
	std::thread::id m_MainThreadId;

	memory::Vector<std::function<void()>> m_MainThreadQueue;
	memory::Vector<std::function<void()>> m_NextTickQueue;
	std::mutex m_MainThreadQueueMutex;

	friend int ::main(int argc, char** argv);
};

// to be defined in client
Application* CreateApplication(ApplicationCommandLineArgs args);

_WHIP_END
