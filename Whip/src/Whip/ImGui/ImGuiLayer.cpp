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
	style.WindowPadding = ImVec2(10.0f, 10.0f);
	style.WindowRounding = 6.0f;
	style.WindowBorderSize = 1.0f;
	style.WindowMinSize = ImVec2(32.0f, 32.0f);
	style.WindowTitleAlign = ImVec2(0.0f, 0.5f);
	style.WindowMenuButtonPosition = ImGuiDir_Left;
	style.ChildRounding = 6.0f;
	style.ChildBorderSize = 1.0f;
	style.PopupRounding = 6.0f;
	style.PopupBorderSize = 1.0f;
	style.FramePadding = ImVec2(8.0f, 5.0f);
	style.FrameRounding = 5.0f;
	style.FrameBorderSize = 0.0f;
	style.ItemSpacing = ImVec2(8.0f, 6.0f);
	style.ItemInnerSpacing = ImVec2(6.0f, 5.0f);
	style.CellPadding = ImVec2(8.0f, 4.0f);
	style.IndentSpacing = 21.0f;
	style.ColumnsMinSpacing = 6.0f;
	style.ScrollbarSize = 12.0f;
	style.ScrollbarRounding = 6.0f;
	style.GrabMinSize = 10.0f;
	style.GrabRounding = 4.0f;
	style.LogSliderDeadzone = 3.0f;
	style.TabRounding = 4.0f;
	style.TabBorderSize = 0.0f;
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
	style.DockingSeparatorSize = 1.0f;
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
	style.Colors[ImGuiCol_Text] = ImVec4(0.89f, 0.91f, 0.92f, 1.0f);
	style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.54f, 0.57f, 1.0f);
	style.Colors[ImGuiCol_WindowBg] = ImVec4(0.055f, 0.059f, 0.066f, 0.98f);
	style.Colors[ImGuiCol_ChildBg] = ImVec4(0.070f, 0.075f, 0.083f, 1.0f);
	style.Colors[ImGuiCol_PopupBg] = ImVec4(0.062f, 0.066f, 0.074f, 0.98f);
	style.Colors[ImGuiCol_Border] = ImVec4(0.145f, 0.158f, 0.175f, 1.0f);
	style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
	style.Colors[ImGuiCol_FrameBg] = ImVec4(0.095f, 0.104f, 0.116f, 1.0f);
	style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.120f, 0.140f, 0.152f, 1.0f);
	style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.140f, 0.170f, 0.180f, 1.0f);
	style.Colors[ImGuiCol_TitleBg] = ImVec4(0.064f, 0.068f, 0.075f, 1.0f);
	style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.075f, 0.081f, 0.090f, 1.0f);
	style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.050f, 0.054f, 0.060f, 1.0f);
	style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.062f, 0.066f, 0.073f, 1.0f);
	style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.052f, 0.056f, 0.062f, 1.0f);
	style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.190f, 0.210f, 0.225f, 1.0f);
	style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.245f, 0.270f, 0.290f, 1.0f);
	style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.310f, 0.345f, 0.365f, 1.0f);
	style.Colors[ImGuiCol_CheckMark] = ImVec4(0.120f, 0.640f, 0.590f, 1.0f);
	style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.120f, 0.580f, 0.540f, 1.0f);
	style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.160f, 0.700f, 0.640f, 1.0f);
	style.Colors[ImGuiCol_Button] = ImVec4(0.110f, 0.118f, 0.130f, 1.0f);
	style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.140f, 0.162f, 0.172f, 1.0f);
	style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.105f, 0.330f, 0.315f, 1.0f);
	style.Colors[ImGuiCol_Header] = ImVec4(0.095f, 0.112f, 0.122f, 1.0f);
	style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.130f, 0.185f, 0.185f, 1.0f);
	style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.110f, 0.360f, 0.340f, 1.0f);
	style.Colors[ImGuiCol_Separator] = ImVec4(0.170f, 0.185f, 0.200f, 1.0f);
	style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.120f, 0.580f, 0.540f, 1.0f);
	style.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.160f, 0.700f, 0.640f, 1.0f);
	style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.120f, 0.580f, 0.540f, 0.28f);
	style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.160f, 0.700f, 0.640f, 0.67f);
	style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.160f, 0.700f, 0.640f, 0.92f);
	style.Colors[ImGuiCol_Tab] = ImVec4(0.082f, 0.090f, 0.100f, 1.0f);
	style.Colors[ImGuiCol_TabHovered] = ImVec4(0.125f, 0.160f, 0.166f, 1.0f);
	style.Colors[ImGuiCol_TabActive] = ImVec4(0.104f, 0.135f, 0.145f, 1.0f);
	style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.058f, 0.063f, 0.070f, 1.0f);
	style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.078f, 0.090f, 0.100f, 1.0f);
	style.Colors[ImGuiCol_DockingPreview] = ImVec4(0.120f, 0.580f, 0.540f, 0.70f);
	style.Colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.048f, 0.052f, 0.058f, 1.0f);
	style.Colors[ImGuiCol_PlotLines] = ImVec4(0.600f, 0.640f, 0.660f, 1.0f);
	style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.160f, 0.700f, 0.640f, 1.0f);
	style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.820f, 0.640f, 0.280f, 1.0f);
	style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.930f, 0.710f, 0.330f, 1.0f);
	style.Colors[ImGuiCol_TableHeaderBg] = ImVec4(0.075f, 0.083f, 0.092f, 1.0f);
	style.Colors[ImGuiCol_TableBorderStrong] = ImVec4(0.160f, 0.175f, 0.190f, 1.0f);
	style.Colors[ImGuiCol_TableBorderLight] = ImVec4(0.120f, 0.132f, 0.145f, 1.0f);
	style.Colors[ImGuiCol_TableRowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
	style.Colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.0f, 1.0f, 1.0f, 0.035f);
	style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.120f, 0.580f, 0.540f, 0.35f);
	style.Colors[ImGuiCol_DragDropTarget] = ImVec4(0.880f, 0.640f, 0.250f, 0.90f);
	style.Colors[ImGuiCol_NavHighlight] = ImVec4(0.120f, 0.580f, 0.540f, 1.0f);
	style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(0.89f, 0.91f, 0.92f, 0.70f);
	style.Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.35f);
	style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.62f);
}

_WHIP_END
