#include "FBoxApp2D.h"
#include <Whip/Core/EntryPoint.h>
#include <Whip/Scene/SceneSerializer.h>
#include <Whip/Scripting/ScriptEngine.h>
#include <Whip/Asset/AssetManager.h>
#include <Whip/UI/UIHelpers.h>

#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>

FBoxApp2D::FBoxApp2D() : Layer("Fbox2D") {}

void FBoxApp2D::OnAttach()
{
	whip::FramebufferSpecification fbSpec{};
	fbSpec.m_Attachments = { whip::FramebufferTextureFormat::Rgba8, whip::FramebufferTextureFormat::Depth };
	fbSpec.m_Width = (uint32_t)m_ViewportSize.x;
	fbSpec.m_Height = (uint32_t)m_ViewportSize.y;
	m_Framebuffer = whip::Framebuffer::Create(fbSpec);

	auto commandLineArgs = whip::Application::Get().GetSpecification().m_CommandLineArgs;
	if (commandLineArgs.m_Count > 1)
	{
		auto projectFilepath = commandLineArgs[1];
		if (LoadProject(projectFilepath))
		{
			LoadStartScene();
		}
		else
		{
			WHP_CORE_ERROR("Failed to load Project: {0}", projectFilepath);
			whip::Application::Get().Close();
		}
	}
	else
	{
		if (LoadProject(whip::FileDialogs::OpenFile("Whip Project (*.wproj)\0*.wproj\0")))
		{
			LoadStartScene();
		}
	}
}

void FBoxApp2D::OnDetach()
{
	if (m_RuntimeScene)
		m_RuntimeScene->OnRuntimeStop();
}

void FBoxApp2D::OnUpdate(whip::Timestep timestep)
{
	if (!m_SceneLoaded || !m_RuntimeScene)
		return;

	// Viewport Resize
	if (whip::FramebufferSpecification spec = m_Framebuffer->GetSpecification();
		m_ViewportSize.x > 0.0f && m_ViewportSize.y > 0.0f &&
		(spec.m_Width != m_ViewportSize.x || spec.m_Height != m_ViewportSize.y))
	{
		m_Framebuffer->Resize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
		m_RuntimeScene->OnViewportResize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
	}

	// Render
	whip::Renderer2D::ResetStats();
	m_Framebuffer->Bind();
	whip::RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1.0f });
	whip::RenderCommand::Clear();

	// Runtime update
	m_RuntimeScene->OnUpdateRuntime(timestep);

	m_Framebuffer->Unbind();
}

void FBoxApp2D::OnImGuiRender()
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

	ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
	m_ViewportSize = { viewportPanelSize.x, viewportPanelSize.y };

	// Draw Framebuffer
	if (m_Framebuffer)
	{
		whip::UI::Image(
			whip::UI::ToImGuiTextureId(m_Framebuffer->GetColorAttachmentRendererId()),
			viewportPanelSize,
			ImVec2{ 0.0f, 1.0f },
			ImVec2{ 1.0f, 0.0f });
	}

	ImGui::End();
}

void FBoxApp2D::OnEvent(whip::event& eventIn)
{
	WHP_UNUSED(eventIn);
	// Runtime viewport currently forwards no custom Layer events.
}

bool FBoxApp2D::LoadProject(const std::filesystem::path& projectPath)
{
	if (whip::Project::Load(projectPath))
	{
		whip::ScriptEngine::Init();
		return true;
	}
	return false;
}

void FBoxApp2D::LoadStartScene()
{
	whip::AssetHandle startSceneHandle = whip::Project::GetActive()->GetConfig().m_StartScene;

	if (!startSceneHandle)
	{
		WHP_CORE_ERROR("No start scene specified in Project!");
		return;
	}

	whip::Ref<whip::Scene> loadedScene = whip::AssetManager::GetAsset<whip::Scene>(startSceneHandle);
	m_RuntimeScene = whip::Scene::Copy(loadedScene);
	m_RuntimeScene->OnViewportResize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);

	m_RuntimeScene->OnRuntimeStart();
	m_SceneLoaded = true;

	WHP_CORE_INFO("Scene loaded and started!");
}
