#include <whippch.h>
#include <Whip/ImGui/ImGuiLayer.h>
#include <Whip/Core/KeyCodes.h>
#include <Whip/Core/Application.h>

#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_glfw.h" 

// temp
#include <GLFW/glfw3.h>
#include <glad/glad.h>

//#define IMGUI_IMPL_API

_WHIP_START

imgui_layer::imgui_layer() : layer("imgui_layer") {}


imgui_layer::~imgui_layer()
{
	//
}

void imgui_layer::begin()
{
	WHP_PROFILE_FUNCTION();

	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
}

void imgui_layer::end()
{
	WHP_PROFILE_FUNCTION();

	ImGuiIO& io = GET_IM_IO;
	io.DisplaySize = ImVec2((float)application::get().get_window().get_width(), (float)application::get().get_window().get_height());

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

void imgui_layer::on_attach()
{
	WHP_PROFILE_FUNCTION();

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = GET_IM_IO; (void)io;

	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;		//Enable keyboard controls
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;		//Enable gamepad controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;			//Enable docking
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;			//Enable multi-viewport / platform windows

	ImGui::StyleColorsDark();
	

	// When viewports are enabled we tweak WindowsRounding/WindowBg so platform windows can look identical to regular ones.
	ImGuiStyle& gui_style = ImGui::GetStyle();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		gui_style.WindowRounding = 0.0f;
		gui_style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}
	GLFWwindow* window = static_cast<GLFWwindow*>(application::get().get_window().get_native_window());
	// Setup platform/renderer bindings
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 410");
}

void imgui_layer::on_detach()
{
	WHP_PROFILE_FUNCTION();

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}

void imgui_layer::on_imgui_render()
{
	/*ImGui::Begin("Whip Renderer");
	ImGui::Text("OpenGl Vendor: %s", (const char*)glGetString(GL_VENDOR));
	ImGui::Text("OpenGl Renderer: %s", (const char*)glGetString(GL_RENDERER));
	ImGui::Text("OpenGl Version: %s", (const char*)glGetString(GL_VERSION));
	ImGui::End();*/
}

_WHIP_END