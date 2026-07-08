#include "WhipPlayerLayer.h"

#include <Whip/Asset/AssetManager.h>
#include <Whip/Project/PlayerConfig.h>
#include <Whip/Scripting/ScriptEngine.h>
#include <Whip/Utils/PlatformUtils.h>

#include <imgui.h>

#include <algorithm>
#include <cstdint>
#include <string_view>

WhipPlayerLayer::WhipPlayerLayer()
	: Layer("WhipPlayer")
{
}

void WhipPlayerLayer::OnAttach()
{
	const std::filesystem::path executableDirectory = whip::Utils::GetExecutableDirectory();
	m_ConfigPath = whip::PlayerConfigSerializer::GetDefaultConfigPath(executableDirectory);
	m_LogFilePath = std::filesystem::current_path() / "log" / "client.log";

	whip::FramebufferSpecification framebufferSpec{};
	framebufferSpec.m_Attachments = { whip::FramebufferTextureFormat::Rgba8, whip::FramebufferTextureFormat::Depth };
	framebufferSpec.m_Width = static_cast<uint32_t>(m_ViewportSize.x);
	framebufferSpec.m_Height = static_cast<uint32_t>(m_ViewportSize.y);
	m_Framebuffer = whip::Framebuffer::Create(framebufferSpec);

	const std::filesystem::path projectPath = ResolveProjectPath();
	if (projectPath.empty())
	{
		SetFatalError("No Project Configured", "Whip Player could not find a project path. Pass a .wproj path, use --project, or place WhipPlayer.yaml next to the executable.");
		return;
	}

	if (!LoadProject(projectPath) || !LoadStartScene())
		return;
}

void WhipPlayerLayer::OnDetach()
{
	StopRuntimeScene();

	if (whip::Project::GetActive())
		whip::Project::RunState(false);

	whip::ScriptEngine::ClearRuntimeSceneTransitionRequest();
	whip::ScriptEngine::SetRuntimeActiveSceneHandle(0);
}

void WhipPlayerLayer::OnUpdate(whip::Timestep timestep)
{
	whip::Input::SetRuntimeInputEnabled(m_SceneLoaded);
	if (!m_SceneLoaded || !m_RuntimeScene || !m_Framebuffer)
		return;

	const whip::FramebufferSpecification spec = m_Framebuffer->GetSpecification();
	if (m_ViewportSize.x > 0.0f && m_ViewportSize.y > 0.0f &&
		(spec.m_Width != static_cast<uint32_t>(m_ViewportSize.x) || spec.m_Height != static_cast<uint32_t>(m_ViewportSize.y)))
	{
		m_Framebuffer->Resize(static_cast<uint32_t>(m_ViewportSize.x), static_cast<uint32_t>(m_ViewportSize.y));
		m_RuntimeScene->OnViewportResize(static_cast<uint32_t>(m_ViewportSize.x), static_cast<uint32_t>(m_ViewportSize.y));
	}

	whip::Renderer2D::ResetStats();
	m_Framebuffer->Bind();
	whip::RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1.0f });
	whip::RenderCommand::Clear();

	m_RuntimeScene->OnUpdateRuntime(timestep);
	ProcessRuntimeSceneTransition();

	m_Framebuffer->Unbind();
}

void WhipPlayerLayer::OnImGuiRender()
{
	ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoScrollWithMouse |
		ImGuiWindowFlags_NoBringToFrontOnFocus;

	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);

	ImGui::Begin("Game Viewport", nullptr, windowFlags);

	const ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
	m_ViewportSize = { viewportPanelSize.x, viewportPanelSize.y };
	const ImVec2 viewportMin = ImGui::GetCursorScreenPos();
	const ImVec2 viewportMax = ImVec2(viewportMin.x + viewportPanelSize.x, viewportMin.y + viewportPanelSize.y);
	whip::Input::SetViewportState(true, true, { viewportMin.x, viewportMin.y }, { viewportMax.x, viewportMax.y });
	whip::Input::SetRuntimeInputEnabled(m_SceneLoaded);

	if (!m_FatalErrorTitle.empty())
	{
		ImGui::SetCursorPos(ImVec2(42.0f, 42.0f));
		ImGui::BeginGroup();
		ImGui::TextColored(ImVec4(0.95f, 0.42f, 0.34f, 1.0f), "%s", m_FatalErrorTitle.c_str());
		ImGui::Spacing();
		ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + std::max(320.0f, viewportPanelSize.x - 84.0f));
		ImGui::TextWrapped("%s", m_FatalErrorDetail.c_str());
		ImGui::PopTextWrapPos();
		ImGui::Spacing();
		ImGui::TextDisabled("Config: %s", m_ConfigPath.empty() ? "-" : m_ConfigPath.string().c_str());
		ImGui::TextDisabled("Log: %s", m_LogFilePath.empty() ? "-" : m_LogFilePath.string().c_str());
		ImGui::Spacing();
		if (ImGui::Button("Close", ImVec2(120.0f, 30.0f)))
			whip::Application::Get().Close();
		ImGui::EndGroup();
	}
	else if (m_Framebuffer)
	{
		ImGui::Image(
			static_cast<ImTextureID>(static_cast<uint64_t>(m_Framebuffer->GetColorAttachmentRendererId())),
			viewportPanelSize,
			ImVec2{ 0.0f, 1.0f },
			ImVec2{ 1.0f, 0.0f });
	}

	ImGui::End();
}

void WhipPlayerLayer::OnEvent(whip::Event& eventIn)
{
	WHP_UNUSED(eventIn);
}

std::filesystem::path WhipPlayerLayer::ResolveProjectPath() const
{
	const whip::ApplicationCommandLineArgs args = whip::Application::Get().GetSpecification().m_CommandLineArgs;
	const std::filesystem::path executableDirectory = whip::Utils::GetExecutableDirectory();
	for (int i = 1; i < args.m_Count; ++i)
	{
		std::string_view arg = args[i];
		if (arg == "--project" && i + 1 < args.m_Count)
			return args[i + 1];

		if (!arg.starts_with("--"))
			return args[i];
	}

	whip::PlayerConfig config;
	whip::PlayerConfigSerializer serializer(config);
	if (serializer.Deserialize(whip::PlayerConfigSerializer::GetDefaultConfigPath(executableDirectory)))
		return config.m_ProjectPath.is_absolute() ? config.m_ProjectPath : executableDirectory / config.m_ProjectPath;

	return {};
}

void WhipPlayerLayer::SetFatalError(std::string title, std::string detail)
{
	m_FatalErrorTitle = std::move(title);
	m_FatalErrorDetail = std::move(detail);
	WHP_CORE_ERROR("[Whip Player] {0}: {1}", m_FatalErrorTitle, m_FatalErrorDetail);
}

bool WhipPlayerLayer::LoadProject(const std::filesystem::path& projectPath)
{
	if (projectPath.empty() || !std::filesystem::exists(projectPath))
	{
		SetFatalError("Project File Missing", "Project file does not exist: " + projectPath.string());
		return false;
	}

	if (!whip::Project::Load(projectPath))
	{
		SetFatalError("Project Load Failed", "Whip could not load the project file: " + projectPath.string());
		return false;
	}

	whip::Project::RunState(true);
	whip::ScriptEngine::Init();
	WHP_CORE_INFO("[Whip Player] Project loaded: {0}", projectPath.string());
	return true;
}

bool WhipPlayerLayer::LoadStartScene()
{
	if (!whip::Project::GetActive())
		return false;

	const whip::AssetHandle startSceneHandle = whip::Project::GetActive()->GetConfig().m_StartScene;
	if (!startSceneHandle)
	{
		SetFatalError("Start Scene Missing", "The project has no start scene configured.");
		return false;
	}

	return LoadRuntimeScene(startSceneHandle);
}

bool WhipPlayerLayer::LoadRuntimeScene(whip::AssetHandle sceneHandle)
{
	if (!whip::Project::GetActive() || sceneHandle == 0)
		return false;

	const whip::Ref<whip::Project> activeProject = whip::Project::GetActive();
	if (!activeProject->GetRuntimeAssetManager()->IsAssetHandleValid(sceneHandle) ||
		activeProject->GetRuntimeAssetManager()->GetAssetType(sceneHandle) != whip::AssetType::Scene)
	{
		SetFatalError("Scene Handle Invalid", "Runtime scene load failed. Invalid scene handle: " + std::to_string(static_cast<uint64_t>(sceneHandle)));
		return false;
	}

	whip::Ref<whip::Scene> sourceScene = whip::AssetManager::GetAsset<whip::Scene>(sceneHandle);
	if (!sourceScene)
	{
		SetFatalError("Scene Load Failed", "Runtime scene asset could not be loaded.");
		return false;
	}

	StopRuntimeScene();
	m_RuntimeScene = whip::Scene::Copy(sourceScene);
	m_RuntimeScene->OnViewportResize(static_cast<uint32_t>(m_ViewportSize.x), static_cast<uint32_t>(m_ViewportSize.y));
	m_ActiveSceneHandle = sceneHandle;
	whip::ScriptEngine::SetRuntimeActiveSceneHandle(sceneHandle);
	m_RuntimeScene->OnRuntimeStart();
	whip::ScriptEngine::SetRuntimeActiveSceneHandle(sceneHandle);
	m_SceneLoaded = true;

	WHP_CORE_INFO("[Whip Player] Runtime scene loaded.");
	return true;
}

bool WhipPlayerLayer::UnloadRuntimeScene()
{
	StopRuntimeScene();
	m_RuntimeScene = whip::MakeRef<whip::Scene>();
	m_RuntimeScene->OnViewportResize(static_cast<uint32_t>(m_ViewportSize.x), static_cast<uint32_t>(m_ViewportSize.y));
	m_SceneLoaded = false;
	WHP_CORE_INFO("[Whip Player] Runtime scene unloaded.");
	return true;
}

void WhipPlayerLayer::StopRuntimeScene()
{
	if (m_RuntimeScene && m_SceneLoaded)
		m_RuntimeScene->OnRuntimeStop();

	m_RuntimeScene = nullptr;
	m_SceneLoaded = false;
	m_ActiveSceneHandle = 0;
	whip::ScriptEngine::SetRuntimeActiveSceneHandle(0);
}

void WhipPlayerLayer::ProcessRuntimeSceneTransition()
{
	const whip::RuntimeSceneTransitionRequest request = whip::ScriptEngine::ConsumeRuntimeSceneTransitionRequest();
	switch (request.m_Type)
	{
	case whip::RuntimeSceneTransitionType::Load:
		LoadRuntimeScene(request.m_SceneHandle);
		break;
	case whip::RuntimeSceneTransitionType::Reload:
		LoadRuntimeScene(request.m_SceneHandle != 0 ? request.m_SceneHandle : m_ActiveSceneHandle);
		break;
	case whip::RuntimeSceneTransitionType::Unload:
		UnloadRuntimeScene();
		break;
	case whip::RuntimeSceneTransitionType::None:
		break;
	}
}
