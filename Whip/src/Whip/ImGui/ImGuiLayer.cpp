#include <WhipPch.h>
#include <Whip/ImGui/ImGuiLayer.h>

#include <Whip/Core/KeyCodes.h>
#include <Whip/Core/Application.h>
#include <Whip/UI/UISettings.h>

#include <imgui.h>
#include <imgui_internal.h>

#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_glfw.h>

#include <GLFW/glfw3.h>
#include <glad/glad.h>

_WHIP_START

ImGuiLayer::ImGuiLayer() : Layer("ImGuiLayer") {}


ImGuiLayer::~ImGuiLayer()
{
	//
}

void ImGuiLayer::OnAttach()
{
	WHP_PROFILE_FUNCTION();

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.IniFilename = "WhipEditorLayout.ini";

	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;		//Enable keyboard controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;			//Enable docking
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;			//Enable multi-viewport / platform windows

	float fontSize = 18.0f;
	io.Fonts->AddFontFromFileTTF("assets/fonts/opensans/OpenSans-Bold.ttf", fontSize);
	io.FontDefault = io.Fonts->AddFontFromFileTTF("assets/fonts/opensans/OpenSans-Regular.ttf", fontSize);

	SetInitialStyle();
	SetDarkThemeColor();

	GLFWwindow* window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 410");
}

void ImGuiLayer::OnDetach()
{
	WHP_PROFILE_FUNCTION();

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}

void ImGuiLayer::OnEvent(Event& event)
{
	if (m_BlockEvents)
	{
		ImGuiIO& io = ImGui::GetIO();
		event.m_Handled |= event.IsInCategory(EventCategoryMouse) & io.WantCaptureMouse;
		event.m_Handled |= event.IsInCategory(EventCategoryKeyboard) & io.WantCaptureKeyboard;
	}
}

void ImGuiLayer::Begin()
{
	WHP_PROFILE_FUNCTION();

	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
}

void ImGuiLayer::End()
{
	WHP_PROFILE_FUNCTION();

	ImGuiIO& io = ImGui::GetIO();
	io.DisplaySize = ImVec2((float)Application::Get().GetWindow().GetWidth(), (float)Application::Get().GetWindow().GetHeight());

	// RENDERING
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		GLFWwindow* backupContext = glfwGetCurrentContext();
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
		glfwMakeContextCurrent(backupContext);
	}
}


uint32_t ImGuiLayer::GetActiveWidgetID() const
{
	return GImGui->ActiveId;
}

void ImGuiLayer::SetInitialStyle()
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

void ImGuiLayer::SetDarkThemeColor()
{
	UI::ApplyEditorTheme(UI::EditorTheme::WhipDark);
}

_WHIP_END
