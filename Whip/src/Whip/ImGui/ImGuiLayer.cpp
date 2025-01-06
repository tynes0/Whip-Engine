#include <whippch.h>
#include "ImGuiLayer.h"

#include <Whip/Core/KeyCodes.h>
#include <Whip/Core/Application.h>

#include <imgui.h>
#include <imgui_internal.h>

#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_glfw.h> 

#include <GLFW/glfw3.h>
#include <glad/glad.h>

_WHIP_START

imgui_layer::imgui_layer() : layer("imgui_layer") {}


imgui_layer::~imgui_layer()
{
	//
}

void imgui_layer::on_attach()
{
	WHP_PROFILE_FUNCTION();

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;

	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;		//Enable keyboard controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;			//Enable docking
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;			//Enable multi-viewport / platform windows

	float font_size = 18.0f;
	io.Fonts->AddFontFromFileTTF("assets/fonts/opensans/OpenSans-Bold.ttf", font_size);
	io.FontDefault = io.Fonts->AddFontFromFileTTF("assets/fonts/opensans/OpenSans-Regular.ttf", font_size);

	set_initial_style();
	set_dark_theme_color();

	GLFWwindow* window = static_cast<GLFWwindow*>(application::get().get_window().get_native_window());
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

void imgui_layer::on_event(event& evnt)
{
	if (m_block_events)
	{
		ImGuiIO& io = ImGui::GetIO();
		evnt.handled |= evnt.is_in_category(event_category_mouse) & io.WantCaptureMouse;
		evnt.handled |= evnt.is_in_category(event_category_keyboard) & io.WantCaptureKeyboard;
	}
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

	ImGuiIO& io = ImGui::GetIO();
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


uint32_t imgui_layer::get_active_widgetID() const
{
	return GImGui->ActiveId;
}

void imgui_layer::set_initial_style()
{
	ImGuiStyle& style = ImGui::GetStyle();
	style.Alpha = 1.0f;
	style.DisabledAlpha = 0.6f;
	style.WindowPadding = ImVec2(8.0f, 8.0f);
	style.WindowRounding = 10.0f;
	style.WindowBorderSize = 1.0f;
	style.WindowMinSize = ImVec2(32.0f, 32.0f);
	style.WindowTitleAlign = ImVec2(0.0f, 0.5f);
	style.WindowMenuButtonPosition = ImGuiDir_Left;
	style.ChildRounding = 10.0f;
	style.ChildBorderSize = 1.0f;
	style.PopupRounding = 10.0f;
	style.PopupBorderSize = 1.0f;
	style.FramePadding = ImVec2(4.0f, 3.0f);
	style.FrameRounding = 3.0f;
	style.FrameBorderSize = 0.0f;
	style.ItemSpacing = ImVec2(8.0f, 4.0f);
	style.ItemInnerSpacing = ImVec2(4.0f, 4.0f);
	style.CellPadding = ImVec2(4.0f, 2.0f);
	style.IndentSpacing = 21.0f;
	style.ColumnsMinSpacing = 6.0f;
	style.ScrollbarSize = 14.0f;
	style.ScrollbarRounding = 9.0f;
	style.GrabMinSize = 10.0f;
	style.GrabRounding = 3.0f;
	style.LogSliderDeadzone = 3.0f;
	style.TabRounding = 5.0f;
	style.TabBorderSize = 1.0f;
	style.TabMinWidthForCloseButton = 0.0f;
	style.ColorButtonPosition = ImGuiDir_Right;
	style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
	style.SelectableTextAlign = ImVec2(0.0f, 0.0f);
	style.TabBarBorderSize = 1.0f;
	style.TableAngledHeadersAngle = 35.0f * (IM_PI / 180.0f);
	style.TableAngledHeadersTextAlign = ImVec2(0.5f, 0.0f);
	style.SeparatorTextBorderSize = 3.0f;
	style.SeparatorTextAlign = ImVec2(0.0f, 0.5f);
	style.SeparatorTextPadding = ImVec2(20.0f, 3.f);
	style.DisplayWindowPadding = ImVec2(19, 19);
	style.DisplaySafeAreaPadding = ImVec2(3, 3);
	style.DockingSeparatorSize = 2.0f;
	style.MouseCursorScale = 1.0f;
	style.AntiAliasedLines = true;
	style.AntiAliasedLinesUseTex = true;
	style.AntiAliasedFill = true;
	style.CurveTessellationTol = 1.25f;
	style.CircleTessellationMaxError = 0.30f;

	style.HoverStationaryDelay = 0.15f;
	style.HoverDelayShort = 0.15f;
	style.HoverDelayNormal = 0.40f;
	style.HoverFlagsForTooltipMouse = ImGuiHoveredFlags_Stationary | ImGuiHoveredFlags_DelayShort | ImGuiHoveredFlags_AllowWhenDisabled;
	style.HoverFlagsForTooltipNav = ImGuiHoveredFlags_NoSharedDelay | ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_AllowWhenDisabled;

}

void imgui_layer::set_dark_theme_color()
{
	ImGuiStyle& style = ImGui::GetStyle();
	style.Colors[ImGuiCol_Text] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
	style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.73f, 0.75f, 0.74f, 1.0f);
	style.Colors[ImGuiCol_WindowBg] = ImVec4(0.086f, 0.086f, 0.086f, 0.94f);
	style.Colors[ImGuiCol_ChildBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
	style.Colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
	style.Colors[ImGuiCol_Border] = ImVec4(0.14f, 0.14f, 0.14f, 1.0f);
	style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.14f, 0.14f, 0.14f, 1.0f);
	style.Colors[ImGuiCol_FrameBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.5f);
	style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.2f, 0.2f, 0.2f, 0.4f);
	style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.2f, 0.2f, 0.2f, 0.67f);
	style.Colors[ImGuiCol_TitleBg] = ImVec4(0.14f, 0.14f, 0.14f, 0.65f);
	style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.14f, 0.14f, 0.14f, 1.0f);
	style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.14f, 0.14f, 0.14f, 0.67f);
	style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
	style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.5f);
	style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.3f, 0.3f, 0.3f, 1.0f);
	style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);
	style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
	style.Colors[ImGuiCol_CheckMark] = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
	style.Colors[ImGuiCol_SliderGrab] = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
	style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(1.0f, 0.38f, 0.38f, 1.0f);
	style.Colors[ImGuiCol_Button] = ImVec4(0.15f, 0.15f, 0.15f, 0.75f);
	style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.2f, 0.2f, 0.2f, 0.4f);
	style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.2f, 0.2f, 0.2f, 0.67f);
	style.Colors[ImGuiCol_Header] = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
	style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.27f, 0.27f, 0.27f, 1.0f);
	style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.35f, 0.35f, 0.35f, 1.0f);
	style.Colors[ImGuiCol_Separator] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
	style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
	style.Colors[ImGuiCol_SeparatorActive] = ImVec4(1.0f, 0.33f, 0.33f, 1.0f);
	style.Colors[ImGuiCol_ResizeGrip] = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
	style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(1.0f, 0.5f, 0.5f, 1.0f);
	style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(1.0f, 0.5f, 0.5f, 1.0f);
	style.Colors[ImGuiCol_Tab] = ImVec4(0.22f, 0.22f, 0.22f, 1.0f);
	style.Colors[ImGuiCol_TabHovered] = ImVec4(0.29f, 0.29f, 0.29f, 1.0f);
	style.Colors[ImGuiCol_TabActive] = ImVec4(0.4f, 0.15f, 0.15f, 1.0f);
	style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.15f, 0.06f, 0.06f, 0.1f);
	style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.3f, 0.17f, 0.17f, 1.0f);
	style.Colors[ImGuiCol_DockingPreview] = ImVec4(0.35f, 0.35f, 0.35f, 0.7f);
	style.Colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
	style.Colors[ImGuiCol_PlotLines] = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
	style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
	style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.9f, 0.0f, 0.0f, 1.0f);
	style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.36f, 0.0f, 0.0f, 1.0f);
	style.Colors[ImGuiCol_TableHeaderBg] = ImVec4(0.3f, 0.3f, 0.3f, 1.0f);
	style.Colors[ImGuiCol_TableBorderStrong] = ImVec4(0.14f, 0.14f, 0.14f, 1.0f);
	style.Colors[ImGuiCol_TableBorderLight] = ImVec4(0.14f, 0.14f, 0.14f, 1.0f);
	style.Colors[ImGuiCol_TableRowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
	style.Colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.0f, 1.0f, 1.0f, 0.06f);
	style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.63f, 0.87f, 0.43f);
	style.Colors[ImGuiCol_DragDropTarget] = ImVec4(0.46f, 0.18f, 0.18f, 0.96f);
	style.Colors[ImGuiCol_NavHighlight] = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);
	style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.0f, 1.0f, 1.0f, 0.7f);
	style.Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.8f, 0.8f, 0.8f, 0.2f);
	style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.8f, 0.8f, 0.8f, 0.35f);
}

_WHIP_END
