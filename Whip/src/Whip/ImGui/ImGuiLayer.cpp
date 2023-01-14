#include <whippch.h>
#include <Whip/ImGui/ImGuiLayer.h>
#include <Whip/KeyCodes.h>
#include <Whip/Application.h>

#include "imgui.h"
#include <Platform/OpenGL/ImGuiOpenGLRenderer.h>


_WHIP_START

ImGuiLayer::ImGuiLayer() : Layer("ImGuiLayer") {}


ImGuiLayer::~ImGuiLayer()
{
	// 
}

void ImGuiLayer::OnAttach()
{
	ImGui::CreateContext();
	ImGui::StyleColorsDark();

	ImGuiIO& io = ImGui::GetIO();
	io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
	io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;

	io.KeyMap[ImGuiKey_Tab] = WHP_KEY_TAB;
	io.KeyMap[ImGuiKey_LeftArrow] = WHP_KEY_LEFT;
	io.KeyMap[ImGuiKey_RightArrow] = WHP_KEY_RIGHT;
	io.KeyMap[ImGuiKey_UpArrow] = WHP_KEY_UP;
	io.KeyMap[ImGuiKey_DownArrow] = WHP_KEY_DOWN;
	io.KeyMap[ImGuiKey_PageUp] = WHP_KEY_PAGE_UP;
	io.KeyMap[ImGuiKey_PageDown] = WHP_KEY_PAGE_DOWN;
	io.KeyMap[ImGuiKey_Home] = WHP_KEY_HOME;
	io.KeyMap[ImGuiKey_End] = WHP_KEY_END;
	io.KeyMap[ImGuiKey_Insert] = WHP_KEY_INSERT;
	io.KeyMap[ImGuiKey_Delete] = WHP_KEY_DELETE;
	io.KeyMap[ImGuiKey_Backspace] = WHP_KEY_BACKSPACE;
	io.KeyMap[ImGuiKey_Space] = WHP_KEY_SPACE;
	io.KeyMap[ImGuiKey_Enter] = WHP_KEY_ENTER;
	io.KeyMap[ImGuiKey_Escape] = WHP_KEY_ESCAPE;
	io.KeyMap[ImGuiKey_A] = WHP_KEY_A;
	io.KeyMap[ImGuiKey_C] = WHP_KEY_C;
	io.KeyMap[ImGuiKey_V] = WHP_KEY_V;
	io.KeyMap[ImGuiKey_X] = WHP_KEY_X;
	io.KeyMap[ImGuiKey_Y] = WHP_KEY_Y;
	io.KeyMap[ImGuiKey_Z] = WHP_KEY_Z;

	ImGui_ImplOpenGL3_Init("#version 410");
}

void ImGuiLayer::OnDetach()
{

}
#include <GLFW/glfw3.h>

void ImGuiLayer::OnUpdate()
{
	ImGuiIO& io = ImGui::GetIO();
	Application& app = Application::Get();
	io.DisplaySize = ImVec2(app.GetWindow().GetWidth(), app.GetWindow().GetHeight());

	ImGui_ImplOpenGL3_NewFrame();
	ImGui::NewFrame();

	float time = (float)glfwGetTime();
	io.DeltaTime = (m_Time > 0.0f) ? (time - m_Time) : (1.0f / 60.0f);
	m_Time = time;

	static bool show = true;
	ImGui::ShowDemoWindow(&show);

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void ImGuiLayer::OnEvent(Event& event)
{
	
}

_WHIP_END