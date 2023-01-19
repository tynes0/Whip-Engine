#include <whippch.h>
#include <Whip/ImGui/ImGuiLayer.h>
#include <Whip/KeyCodes.h>
#include <Whip/Application.h>

#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_glfw.h" 

// temp
#include <GLFW/glfw3.h>
#include <glad/glad.h>

//#define IMGUI_IMPL_API

_WHIP_START

ImGuiLayer::ImGuiLayer() : Layer("ImGuiLayer") {}


ImGuiLayer::~ImGuiLayer()
{
	//
}

void ImGuiLayer::begin()
{
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
}

void ImGuiLayer::end()
{
	ImGuiIO& io = GET_IM_IO;
	io.DisplaySize = ImVec2((float)Application::Get().GetWindow().GetWidth(), (float)Application::Get().GetWindow().GetHeight());

	// RENDERING
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		GLFWwindow* backup_context = glfwGetCurrentContext();
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
		glfwMakeContextCurrent(backup_context);
	}
}

void ImGuiLayer::OnAttach()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = GET_IM_IO;

	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;		//Enable keyboard controls
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;		//Enable gamepad controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;			//Enable docking
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;			//Enable multi-viewport / platform windows

	ImGui::StyleColorsDark();
	

	// When viewports are enabled we tweak WindowsRounding/WindowBg so platform windows can look identical to regular ones.
	ImGuiStyle& GuiStyle = ImGui::GetStyle();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		GuiStyle.WindowRounding = 0.0f;
		GuiStyle.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}
	GLFWwindow* window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
	// Setup platform/renderer bindings
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 410");
}

void ImGuiLayer::OnDetach()
{
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}

void ImGuiLayer::OnImGuiRender()
{
	static bool show = true;
	ImGui::ShowDemoWindow(&show);

	ImGui::Begin("Tynes");
	ImGui::Text("Testing stuff");
	ImGui::End();
}

_WHIP_END