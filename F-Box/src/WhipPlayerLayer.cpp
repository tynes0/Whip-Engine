#include "WhipPlayerLayer.h"

#include <Whip/Asset/AssetManager.h>
#include <Whip/Project/PlayerConfig.h>
#include <Whip/Scripting/ScriptEngine.h>
#include <Whip/Utils/PlatformUtils.h>

#include <imgui.h>

#include <cstdint>
#include <string_view>

WhipPlayerLayer::WhipPlayerLayer()
	: Layer("WhipPlayer")
{
}

void WhipPlayerLayer::OnAttach()
{
	whip::FramebufferSpecification framebufferSpec{};
	framebufferSpec.m_Attachments = { whip::FramebufferTextureFormat::Rgba8, whip::FramebufferTextureFormat::Depth };
	framebufferSpec.m_Width = static_cast<uint32_t>(m_ViewportSize.x);
	framebufferSpec.m_Height = static_cast<uint32_t>(m_ViewportSize.y);
	m_Framebuffer = whip::Framebuffer::Create(framebufferSpec);

	const std::filesystem::path projectPath = ResolveProjectPath();
	if (projectPath.empty())
	{
		WHP_CORE_ERROR("[Whip Player] No project path was provided. Usage: Whip-Player <Project.wproj> or Whip-Player --project <Project.wproj>");
		whip::Application::Get().Close();
		return;
	}

	if (!LoadProject(projectPath) || !LoadStartScene())
		whip::Application::Get().Close();
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

	if (m_Framebuffer)
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

bool WhipPlayerLayer::LoadProject(const std::filesystem::path& projectPath)
{
	if (projectPath.empty() || !std::filesystem::exists(projectPath))
	{
		WHP_CORE_ERROR("[Whip Player] Project file does not exist: {0}", projectPath.string());
		return false;
	}

	if (!whip::Project::Load(projectPath))
	{
		WHP_CORE_ERROR("[Whip Player] Failed to load project: {0}", projectPath.string());
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
		WHP_CORE_ERROR("[Whip Player] No start scene specified in project.");
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
		WHP_CORE_ERROR("[Whip Player] Runtime scene load failed. Invalid scene handle: {0}", static_cast<uint64_t>(sceneHandle));
		return false;
	}

	whip::Ref<whip::Scene> sourceScene = whip::AssetManager::GetAsset<whip::Scene>(sceneHandle);
	if (!sourceScene)
	{
		WHP_CORE_ERROR("[Whip Player] Runtime scene load failed. Scene asset could not be loaded.");
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
