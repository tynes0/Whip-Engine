#include <whippch.h>
#include "ImGuiLayer.h"

#include <Whip/Core/KeyCodes.h>
#include <Whip/Core/Application.h>
#include <Whip/UI/UI_settings.h>

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
	io.IniFilename = "WhipEditorLayout.ini";

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
	style.WindowPadding = ImVec2(10.0f, 8.0f);
	style.WindowRounding = 4.0f;
	style.WindowBorderSize = 1.0f;
	style.WindowMinSize = ImVec2(32.0f, 32.0f);
	style.WindowTitleAlign = ImVec2(0.0f, 0.5f);
	style.WindowMenuButtonPosition = ImGuiDir_Left;
	style.ChildRounding = 4.0f;
	style.ChildBorderSize = 1.0f;
	style.PopupRounding = 4.0f;
	style.PopupBorderSize = 1.0f;
	style.FramePadding = ImVec2(8.0f, 5.0f);
	style.FrameRounding = 3.0f;
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
	style.TabRounding = 3.0f;
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
	UI::apply_editor_theme(UI::editor_theme::whip_dark);
}

_WHIP_END
