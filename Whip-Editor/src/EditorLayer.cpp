#include <Whip-Editor/EditorLayer.h>

#include <Whip/Core/EntryPoint.h>
#include <Whip/Utils/FileExtensions.h>
#include <Whip/Scene/SceneSerializer.h>
#include <Whip/Scripting/ScriptEngine.h>
#include <Whip/Utils/PlatformUtils.h>
#include <Whip-Editor/UI/UIHelpers.h>
#include <Whip-Editor/UI/UIProjectLoader.h>
#include <Whip/Math/Math.h>
#include <Whip/Asset/AssetManager.h>
#include <Whip/Asset/AssetMetadata.h>
#include <Whip/Asset/AssetUtils.h>
#include <Whip/Asset/SceneImporter.h>

#include <Whip-Editor/Helpers/IconManager.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <string>
#include <string_view>

#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <entt.hpp>
#include <ImGuizmo.h>

_WHIP_START

namespace
{
	bool IsControlDown()
	{
		return Input::IsKeyDown(Key::LeftControl) || Input::IsKeyDown(Key::RightControl);
	}

	int GizmoSnapIndex(int operation)
	{
		if (operation == ImGuizmo::OPERATION::TRANSLATE)
			return 0;
		if (operation == ImGuizmo::OPERATION::ROTATE)
			return 1;
		if (operation == ImGuizmo::OPERATION::SCALE || operation == ImGuizmo::OPERATION::SCALEU)
			return 2;
		return -1;
	}

	ImU32 ColorU32(float r, float g, float b, float a = 1.0f)
	{
		return ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, a));
	}

	std::string SanitizeProjectToken(std::string value, const std::string& fallback)
	{
		std::erase_if(value, [](unsigned char c)
		{
			return !std::isalnum(c) && c != '_' && c != '-' && c != ' ';
		});

		while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
			value.erase(value.begin());
		while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
			value.pop_back();

		if (value.empty())
			value = fallback;
		return value;
	}

	std::string SanitizePathToken(std::string value, const std::string& fallback)
	{
		value = SanitizeProjectToken(std::move(value), fallback);
		for (char& c : value)
			if (c == ' ')
				c = '_';
		return value;
	}

	std::filesystem::path NormalizeProjectListPath(const std::filesystem::path& path)
	{
		if (path.empty())
			return {};

		std::error_code error;
		std::filesystem::path normalizedPath = std::filesystem::weakly_canonical(path, error);
		if (error)
		{
			error.clear();
			normalizedPath = std::filesystem::absolute(path, error);
		}

		return error ? path : normalizedPath.lexically_normal();
	}

	bool PathsMatchForRecentProject(const std::filesystem::path& left, const std::filesystem::path& right)
	{
		const std::filesystem::path normalizedLeft = NormalizeProjectListPath(left);
		const std::filesystem::path normalizedRight = NormalizeProjectListPath(right);
		return !normalizedLeft.empty() && normalizedLeft == normalizedRight;
	}

	bool PathIsOrIsUnder(const std::filesystem::path& path, const std::filesystem::path& directory)
	{
		const std::filesystem::path normalizedPath = path.lexically_normal();
		const std::filesystem::path normalizedDirectory = directory.lexically_normal();
		if (normalizedPath == normalizedDirectory)
			return true;

		auto pathIt = normalizedPath.begin();
		auto directoryIt = normalizedDirectory.begin();
		for (; directoryIt != normalizedDirectory.end(); ++directoryIt, ++pathIt)
		{
			if (pathIt == normalizedPath.end() || *pathIt != *directoryIt)
				return false;
		}

		return true;
	}

	bool CreateDirectoryChecked(const std::filesystem::path& path, std::string_view label)
	{
		std::error_code error;
		std::filesystem::create_directories(path, error);
		if (!error)
			return true;

		WHP_EDITOR_ERROR("[Whip Hub] Could not create {}: {} ({})", label, path.string(), error.message());
		return false;
	}

}

EditorLayer::EditorLayer()
	: Layer("Fbox2D"),
	m_ActiveScene(m_SceneManager.GetActiveSceneStorage()),
	m_EditorScene(m_SceneManager.GetEditorSceneStorage()),
	m_EditorScenePath(m_SceneManager.GetEditorScenePathStorage()),
	m_GizmoHistoryActive(m_HistoryManager.GetGizmoHistoryActiveStorage()),
	m_SceneDirty(m_SceneManager.GetDirtyStorage()),
	m_LastSceneRecoverySnapshot(m_SceneManager.GetLastRecoverySnapshotStorage()),
	m_SceneState(m_SceneManager.GetStateStorage()),
	m_EditorCamera(),
	m_GizmoType(ImGuizmo::OPERATION::TRANSLATE)
{
}

void EditorLayer::OnAttach()
{
    WHP_PROFILE_FUNCTION();
	WHP_EDITOR_INFO("[Editor] Attaching EditorLayer.");

	m_AnimationEditorPanel.SetRefreshAssetTreeCallback([this]() {if (m_ContentBrowserPanel) { m_ContentBrowserPanel->RefreshAssetTree(); } });
	m_AssetEditorPanel.SetOpenSceneCallback([this](AssetHandle handle) { OpenScene(handle); });
	m_AssetEditorPanel.SetSetStartSceneCallback([this](AssetHandle handle) { m_AssetInteractionManager.SetStartScene(handle); });
	m_AssetEditorPanel.SetOpenAnimationCallback([this](AssetHandle handle) { return m_AnimationEditorPanel.OpenAsset(handle, false); });
	m_AssetEditorPanel.SetDrawAnimationEditorCallback([this]() { m_AnimationEditorPanel.OnImGuiRenderEmbedded(); });
	m_AssetEditorPanel.SetRefreshAssetTreeCallback([this]() { if (m_ContentBrowserPanel) { m_ContentBrowserPanel->RefreshAssetTree(); } });
	m_SceneHierarchyPanel.SetSceneChangeCallback([this]() { m_HistoryManager.CaptureSceneHistory(*this); });
	m_SceneHierarchyPanel.SetSaveEntityTemplateCallback([this](Entity entityIn) { SaveEntityTemplate(entityIn); });
	m_SceneHierarchyPanel.SetApplyEntityTemplateCallback([this](Entity entityIn) { ApplyEntityTemplate(entityIn); });
	m_SceneHierarchyPanel.SetRevertEntityTemplateCallback([this](Entity entityIn) { RevertEntityTemplate(entityIn); });
	m_SceneHierarchyPanel.SetUnpackEntityTemplateCallback([this](Entity entityIn) { UnpackEntityTemplate(entityIn); });
	m_UIProject.SetSceneCallbacks(
		[this](AssetHandle handle) { OpenScene(handle); },
		[this]() { CloseScene(); },
		[this]() { return m_EditorScenePath; });
	m_UIProject.SetBeforeChangeCallback([this]() { m_HistoryManager.CaptureSceneHistory(*this, true); });
	m_UIProject.SetEditorSettingsDrawer([this]() { m_UISettings.DrawContent(); });
	m_ProjectManager.SetupProjectLoader(*this);
	m_ProjectManager.LoadEditorPreferences(*this);
	m_ProjectManager.GetLoader().SetRecentProjects(m_ProjectManager.GetRecentProjectsStorage());

	// framebuffer
    FramebufferSpecification fbSpec{};
    fbSpec.m_Attachments = { FramebufferTextureFormat::Rgba8, FramebufferTextureFormat::RedInteger, FramebufferTextureFormat::Depth };
    fbSpec.m_Width = Application::Get().GetWindow().GetWidth();
    fbSpec.m_Height = Application::Get().GetWindow().GetHeight();
    m_Framebuffer = Framebuffer::Create(fbSpec);

	// scene
    m_EditorScene = MakeRef<Scene>();
	m_ActiveScene = m_EditorScene;

	// Project
	auto commandLineArgs = Application::Get().GetSpecification().m_CommandLineArgs;
	if (commandLineArgs.m_Count > 1)
	{
		auto projectFilePath = commandLineArgs[1];
		WHP_EDITOR_INFO(std::string("[Project] Opening project from command line: ") + projectFilePath);
		if (OpenProject(projectFilePath))
			m_ProjectManager.GetLoader().SetLoaded(true);
	}
	// camera
    m_EditorCamera = EditorCamera(30.0f, 1.778f, 0.1f, 1000.0f);
	ConsolePanel::Initialize();
	m_StatisticsPanelAdapter = MakeScope<CallbackEditorPanel>(
		"Statistics",
		[this]() { m_UIStatistics.OnImGuiRender(m_Ts); },
		[this]() { return m_UIStatistics.IsOpen(); },
		[this](bool open) { m_UIStatistics.SetOpen(open); },
		[this]() { return m_UIStatistics.ConsumeOpenDirty(); });
	m_ConsolePanelAdapter = MakeScope<CallbackEditorPanel>(
		"Console",
		[]() { ConsolePanel::OnImGuiRender(); },
		[]() { return ConsolePanel::IsOpen(); },
		[](bool open) { ConsolePanel::SetOpen(open); },
		[]() { return ConsolePanel::ConsumeOpenDirty(); },
		false);
	static float v1 = 0, v2 = 0;
	m_PopupHandler
		.SetPopupName("Popup Testing")
		.SetHeight(300.f)
		.SetWidth(400.f)
		.Add([]() { ImGui::Text("This is a text message for popup testing. Do not mind this Window if you see that."); })
		.Add([]() { static float fv = 0; ImGui::SliderFloat("##Float value", &fv, 0.0f, 10000.0f); })
		.SameLine()
		.Add([]() { static int iv = 0; ImGui::SliderInt("##Int value", &iv, 0, 1000000); })
		.AddDualHandleSlider(0, 100, &v1, &v2)
		.AddButton([this]() { m_PopupHandler.SetShowState(false); }, "Close", 100);

}

void EditorLayer::OnDetach()
{
	WHP_PROFILE_FUNCTION();
	WriteSceneRecoverySnapshot("Editor shutdown");
	m_ScriptManager.StopSourceWatcher();
	m_ProjectManager.SaveEditorPreferences(*this);
	ConsolePanel::Shutdown();

	if (m_SceneState == SceneState::Play)
		m_ActiveScene->OnRuntimeStop();
	else if (m_SceneState == SceneState::Simulate)
		m_ActiveScene->OnSimulationStop();

}

void EditorLayer::OnUpdate(Timestep ts)
{
	WHP_PROFILE_FUNCTION();
	m_Ts = ts;
	m_ScriptManager.ProcessSourceChanges(m_SceneState == SceneState::Edit);
	if (m_SceneDirty && m_SceneState == SceneState::Edit)
	{
		const auto now = std::chrono::steady_clock::now();
		if (m_LastSceneRecoverySnapshot == std::chrono::steady_clock::time_point{} || now - m_LastSceneRecoverySnapshot > std::chrono::seconds(30))
			WriteSceneRecoverySnapshot("Autosave");
	}

	{
		WHP_PROFILE_SCOPE("Viewport Size");
		m_ActiveScene->OnViewportResize(static_cast<uint32_t>(m_ViewportSize.x), static_cast<uint32_t>(m_ViewportSize.y));
		if (FramebufferSpecification spec = m_Framebuffer->GetSpecification();
			m_ViewportSize.x > 0.0f &&
			m_ViewportSize.y > 0.0f &&
			(spec.m_Width != static_cast<uint32_t>(m_ViewportSize.x) || spec.m_Height != static_cast<uint32_t>(m_ViewportSize.y)))
		{
			m_Framebuffer->Resize(static_cast<uint32_t>(m_ViewportSize.x), static_cast<uint32_t>(m_ViewportSize.y));
			m_EditorCamera.SetViewportSize(m_ViewportSize.x, m_ViewportSize.y);
		}
	}

	{
		WHP_PROFILE_SCOPE("scene::OnUpdate");
		Renderer2D::ResetStats();
		m_Framebuffer->Bind();
		RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1.0f });
		RenderCommand::Clear();

		m_Framebuffer->ClearAttachment(1, -1);

		switch (m_SceneState)
		{
		case SceneState::Edit:
		{
			if (!m_GizmoUsing)
				m_EditorCamera.OnUpdate(ts);
			DrawEditorGrid();
			m_ActiveScene->OnUpdateEditor(ts, m_EditorCamera);
			break;
		}
		case SceneState::Play:
		{
			m_ActiveScene->OnUpdateRuntime(ts);
			ProcessRuntimeSceneTransition();
			break;
		}
		case SceneState::Simulate:
		{
			if (!m_GizmoUsing)
				m_EditorCamera.OnUpdate(ts);
			DrawEditorGrid();
			m_ActiveScene->OnUpdateSimulation(ts, m_EditorCamera);
			ProcessRuntimeSceneTransition();
			break;
		}
		}
	}

	{
		WHP_PROFILE_SCOPE("Mouse position track");
		auto [mx, my] = ImGui::GetMousePos();
		mx -= m_ViewportBounds[0].x;
		my -= m_ViewportBounds[0].y;
		glm::vec2 viewportSize = m_ViewportBounds[1] - m_ViewportBounds[0];
		my = viewportSize.y - my;
		int mouseX = static_cast<int>(mx);
		int mouseY = static_cast<int>(my);

		if (mouseX >= 0 && mouseY >= 0 && mouseX < static_cast<int>(viewportSize.x) && mouseY < static_cast<int>(viewportSize.y))
		{
			int pixelData = m_Framebuffer->ReadPixel(1, mouseX, mouseY); // This is taking too much time
			m_HoveredEntity = pixelData == -1 ? Entity() : Entity(static_cast<entt::entity>(pixelData), m_ActiveScene.get());
		}
	}

	OnOverlayRender();

    m_Framebuffer->Unbind();
}

_WHP_PRAGMA_WARNING(push)
_WHP_PRAGMA_WARNING_DISABLE(4312)
void EditorLayer::OnImGuiRender()
{
	WHP_PROFILE_FUNCTION();
	ImGuizmo::BeginFrame();
	m_GizmoHovered = false;
	m_GizmoUsing = false;
	const bool projectLoaded = HasProjectLoaded();
	if (!projectLoaded)
	{
		Application::Get().GetImGuiLayer()->BlockEvents(true);

		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->WorkPos);
		ImGui::SetNextWindowSize(viewport->WorkSize);
		ImGui::SetNextWindowViewport(viewport->ID);

		ImGuiWindowFlags hubHostFlags =
			ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImGui::GetStyleColorVec4(ImGuiCol_WindowBg));
		ImGui::Begin("Whip Hub Host", nullptr, hubHostFlags);
		DrawEditorShellTitlebar(false);
		m_ProjectManager.GetLoader().Run();
		ImGui::End();
		ImGui::PopStyleColor();
		ImGui::PopStyleVar(3);
		return;
	}

	// dockspace
	{
		static bool pOpen = true;
		static ImGuiDockNodeFlags dockspaceFlags = ImGuiDockNodeFlags_None;

		ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDocking;

		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->WorkPos);
		ImGui::SetNextWindowSize(viewport->WorkSize);
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		windowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
		windowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

		if (dockspaceFlags & ImGuiDockNodeFlags_PassthruCentralNode)
			windowFlags |= ImGuiWindowFlags_NoBackground;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("Editor DockSpace", &pOpen, windowFlags);
		ImGui::PopStyleVar(3);
		DrawEditorShellTitlebar(projectLoaded);
		DrawEditorMenuBar(projectLoaded);

		ImGuiIO& io = ImGui::GetIO();
		ImGuiStyle& style = ImGui::GetStyle();
		float minWinSizeX = style.WindowMinSize.x;
		style.WindowMinSize.x = 300.0f;
		if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
		{
			ImGuiID dockspaceId = ImGui::GetID("Editor DockSpace");
			ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), dockspaceFlags);
		}
		style.WindowMinSize.x = minWinSizeX;
	}
	// viewport
	{
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0.0f, 0.0f });
		ImGui::Begin("Viewport");
		auto viewportMinRegion = ImGui::GetWindowContentRegionMin();
		auto viewportMaxRegion = ImGui::GetWindowContentRegionMax();
		auto viewportOffset = ImGui::GetWindowPos();
		m_ViewportBounds[0] = { viewportMinRegion.x + viewportOffset.x, viewportMinRegion.y + viewportOffset.y };
		m_ViewportBounds[1] = { viewportMaxRegion.x + viewportOffset.x, viewportMaxRegion.y + viewportOffset.y };
		m_ViewportFocused = ImGui::IsWindowFocused();
		m_ViewportHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
		Application::Get().GetImGuiLayer()->BlockEvents(!m_ViewportHovered || m_GizmoHovered || m_GizmoUsing);
		ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
		m_ViewportSize = { viewportPanelSize.x, viewportPanelSize.y };

		UI::Image(UI::ToImGuiTextureId(m_Framebuffer->GetColorAttachmentRendererId()), viewportPanelSize, ImVec2{ 0.0f, 1.0f }, ImVec2{ 1.0f, 0.0f });
		if (ImGui::BeginDragDropTarget())
		{
			bool handledDrop = false;
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
			{
				const UI::AssetReferencePayload assetPayload = UI::ReadAssetReferencePayload(payload);
				handledDrop = m_AssetInteractionManager.HandleViewportAssetDrop(*this, assetPayload.m_Handle, assetPayload.m_TextureSpriteIndex);
			}

			if (!handledDrop)
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_PATH"))
				{
					std::filesystem::path RelativePath(std::string(static_cast<const char*>(payload->Data), payload->DataSize));
					std::filesystem::path absolutePath = Project::GetActiveAssetDirectory() / RelativePath;
					if (std::filesystem::is_regular_file(absolutePath))
					{
						AssetHandle handle = Project::GetActive()->GetEditorAssetManager()->GetHandleFromFilepath(RelativePath);
						if (handle == 0 && Utils::TryGetAssetTypeFromFileExtension(RelativePath.extension()) != AssetType::None)
							handle = Project::GetActive()->GetEditorAssetManager()->ImportAsset(RelativePath);
						m_AssetInteractionManager.HandleViewportAssetDrop(*this, handle);
					}
				}
			}
			ImGui::EndDragDropTarget();
		}

		// gizmos
		Entity selectedEntity = m_SceneHierarchyPanel.GetSelectedEntity();
		if (selectedEntity && m_GizmoType != -1 && m_SceneState != SceneState::Play)
		{
		    ImGuizmo::SetDrawlist();
			ImGuizmo::SetOrthographic(false);
			ImGuizmo::AllowAxisFlip(false);
		    ImGuizmo::SetRect(m_ViewportBounds[0].x, m_ViewportBounds[0].y, m_ViewportBounds[1].x - m_ViewportBounds[0].x, m_ViewportBounds[1].y - m_ViewportBounds[0].y);
		    // Camera
		    const glm::mat4& cameraProjection = m_EditorCamera.GetProjection();
		    glm::mat4 cameraView = m_EditorCamera.GetViewMatrix();

		    // Entity transform
		    auto& tc = selectedEntity.GetComponent<TransformComponent>();
		    glm::mat4 transform = tc.GetTransform();
			const glm::vec3 baseTranslation = tc.m_Translation;
			const glm::vec3 baseRotation = tc.m_Rotation;
			const glm::vec3 baseScale = tc.m_Scale;

		    // Snapping
			const int snapIndex = GizmoSnapIndex(m_GizmoType);
		    bool snap = IsControlDown() && snapIndex != -1;

			ImGuizmo::OPERATION operation = static_cast<ImGuizmo::OPERATION>(m_GizmoType);
			ImGuizmo::Manipulate(
				glm::value_ptr(cameraView),
				glm::value_ptr(cameraProjection),
				operation,
				ImGuizmo::LOCAL,
				glm::value_ptr(transform),
				nullptr,
				snap ? const_cast<float*>(glm::value_ptr(m_UISettings.GetSnapValues(static_cast<uint32_t>(snapIndex)))) : nullptr);
			m_GizmoHovered = ImGuizmo::IsOver(operation);
			m_GizmoUsing = ImGuizmo::IsUsing();

		    if (m_GizmoUsing)
		    {
				if (!m_GizmoHistoryActive)
				{
					m_HistoryManager.CaptureSceneHistory(*this);
					m_GizmoHistoryActive = true;
				}

		        glm::vec3 translation, rotation, scale;
				if (!Math::DecomposeTransform(transform, translation, rotation, scale))
					WHP_CLIENT_WARN("Transform Decomposing error!");

		        glm::vec3 deltaTranslation = translation - baseTranslation;
		        glm::vec3 deltaRotation = rotation - baseRotation;
				glm::vec3 scaleRatio = glm::vec3(1.0f);
				scaleRatio.x = baseScale.x != 0.0f ? scale.x / baseScale.x : 1.0f;
				scaleRatio.y = baseScale.y != 0.0f ? scale.y / baseScale.y : 1.0f;
				scaleRatio.z = baseScale.z != 0.0f ? scale.z / baseScale.z : 1.0f;

				std::vector<Entity> selectedEntities = m_SceneHierarchyPanel.GetSelectedEntities();
				if (std::find(selectedEntities.begin(), selectedEntities.end(), selectedEntity) == selectedEntities.end())
					selectedEntities.push_back(selectedEntity);

				for (Entity selected : selectedEntities)
				{
					if (!selected || !selected.HasComponent<TransformComponent>())
						continue;
					if (selected == selectedEntity)
						continue;

					auto& selectedTransform = selected.GetComponent<TransformComponent>();
					selectedTransform.m_Translation += deltaTranslation;
					selectedTransform.m_Rotation += deltaRotation;
					selectedTransform.m_Scale *= scaleRatio;
				}

		        tc.m_Translation = translation;
		        tc.m_Rotation = rotation;
		        tc.m_Scale = scale;
		    }
		}
		if (!m_GizmoUsing)
			m_GizmoHistoryActive = false;
		UIToolbar();
		Application::Get().GetImGuiLayer()->BlockEvents(!m_ViewportHovered || m_GizmoHovered || m_GizmoUsing);
		ImGui::End();
		ImGui::PopStyleVar();
	} // viewport

	m_UIProject.OnImGuiRender(); // should be in dockspace

	ImGui::End(); // dockspace

	// other renders
	RebuildEditorPanelRegistry();
	m_PanelManager.OnImGuiRender();
	m_AnimationEditorPanel.HandleShortcutInput(m_UISettings);
	DrawCommandPalette();
	if (m_UISettings.ConsumeDirty()
		|| m_PanelManager.ConsumeOpenDirty()
		|| (m_ContentBrowserPanel && m_ContentBrowserPanel->ConsumePreferencesDirty()))
		m_ProjectManager.SaveEditorPreferences(*this);
	m_PopupHandler.OnImGuiRender();

}
_WHP_PRAGMA_WARNING(pop)

void EditorLayer::OnEvent(Event& event)
{
	if (m_SceneState == SceneState::Edit && !m_GizmoHovered && !m_GizmoUsing && Application::Get().GetImGuiLayer()->GetActiveWidgetID() == 0)
		m_EditorCamera.OnEvent(event);
    EventDispatcher dispatcher(event);
    dispatcher.Dispatch<KeyPressedEvent>([this](auto&&... args) -> decltype(auto) { return this->OnKeyPressed(std::forward<decltype(args)>(args)...); });
    dispatcher.Dispatch<MouseButtonPressedEvent>([this](auto&&... args) -> decltype(auto) { return this->OnMouseButtonPressed(std::forward<decltype(args)>(args)...); });
	dispatcher.Dispatch<WindowDropEvent>([this](auto&&... args) -> decltype(auto) { return this->OnWindowDrop(std::forward<decltype(args)>(args)...); });
}

bool EditorLayer::OnKeyPressed(KeyPressedEvent& event)
{
    // Shortcuts
    if (event.GetRepeatCount() > 0)
        return false;

    bool control = Input::IsKeyDown(Key::LeftControl) || Input::IsKeyDown(Key::RightControl);
    bool shift = Input::IsKeyDown(Key::LeftShift) || Input::IsKeyDown(Key::RightShift);
    bool alt = Input::IsKeyDown(Key::LeftAlt) || Input::IsKeyDown(Key::RightAlt);
	const bool hasActiveWidget = Application::Get().GetImGuiLayer()->GetActiveWidgetID() != 0;

	for (size_t i = 0; i < UI::UISettings::ActionCount; ++i)
	{
		UI::EditorShortcutAction action = static_cast<UI::EditorShortcutAction>(i);
		if (m_UISettings.ShortcutMatches(action, event.GetKeyCode(), control, shift, alt))
		{
			if (hasActiveWidget &&
				action != UI::EditorShortcutAction::OpenCommandPalette &&
				action != UI::EditorShortcutAction::Play &&
				action != UI::EditorShortcutAction::Simulate &&
				action != UI::EditorShortcutAction::Stop &&
				action != UI::EditorShortcutAction::Pause)
			{
				return false;
			}

			if (m_AnimationEditorPanel.WantsShortcutCapture() && m_AnimationEditorPanel.ShouldConsumeShortcutAction(action))
				return true;
			return ExecuteEditorAction(action);
		}
	}

    return false;
}

bool EditorLayer::ExecuteEditorAction(UI::EditorShortcutAction action)
{
	if (!IsEditorActionAvailable(action))
		return false;

	switch (action)
	{
	case UI::EditorShortcutAction::OpenCommandPalette:
		OpenCommandPalette();
		return true;
	case UI::EditorShortcutAction::OpenSettings:
		m_UIProject.Show(UI::UIProject::UISettings, [this]() { m_ProjectManager.FinishProjectSettings(*this); });
		return true;
	case UI::EditorShortcutAction::OpenProject:
		OpenProject();
		return true;
	case UI::EditorShortcutAction::NewScene:
		NewScene();
		return true;
	case UI::EditorShortcutAction::SaveScene:
		SaveScene();
		return true;
	case UI::EditorShortcutAction::SaveSceneAs:
		SaveSceneAs();
		return true;
	case UI::EditorShortcutAction::SaveProject:
		m_ProjectManager.SaveProject();
		return true;
	case UI::EditorShortcutAction::CloseScene:
		CloseScene();
		return true;
	case UI::EditorShortcutAction::ReloadScripts:
		m_ScriptManager.ReloadAssembly(true, m_SceneState == SceneState::Edit);
		return true;
	case UI::EditorShortcutAction::DuplicateEntity:
		m_HistoryManager.DuplicateSelection(*this);
		return true;
	case UI::EditorShortcutAction::DeleteEntity:
		m_HistoryManager.DeleteSelection(*this);
		return true;
	case UI::EditorShortcutAction::Undo:
		m_HistoryManager.UndoScene(*this);
		return true;
	case UI::EditorShortcutAction::Redo:
		m_HistoryManager.RedoScene(*this);
		return true;
	case UI::EditorShortcutAction::SelectAll:
		m_HistoryManager.SelectAll(*this);
		return true;
	case UI::EditorShortcutAction::Copy:
		m_HistoryManager.CopySelection(*this);
		return true;
	case UI::EditorShortcutAction::Paste:
		m_HistoryManager.PasteSelection(*this);
		return true;
	case UI::EditorShortcutAction::Cut:
		m_HistoryManager.CutSelection(*this);
		return true;
	case UI::EditorShortcutAction::Play:
		if (m_SceneState == SceneState::Edit)
			OnScenePlay();
		else if (m_SceneState == SceneState::Play)
			OnSceneStop();
		return true;
	case UI::EditorShortcutAction::Simulate:
		if (m_SceneState == SceneState::Edit)
			OnSceneSimulate();
		else if (m_SceneState == SceneState::Simulate)
			OnSceneStop();
		return true;
	case UI::EditorShortcutAction::Stop:
		OnSceneStop();
		return true;
	case UI::EditorShortcutAction::Pause:
		m_ActiveScene->SetPaused(!m_ActiveScene->IsPaused());
		return true;
	case UI::EditorShortcutAction::GizmoNone:
		m_GizmoType = -1;
		return true;
	case UI::EditorShortcutAction::GizmoTranslate:
		m_GizmoType = ImGuizmo::OPERATION::TRANSLATE;
		return true;
	case UI::EditorShortcutAction::GizmoRotate:
		m_GizmoType = ImGuizmo::OPERATION::ROTATE;
		return true;
	case UI::EditorShortcutAction::GizmoScale:
		m_GizmoType = ImGuizmo::OPERATION::SCALE;
		return true;
	default:
		return false;
	}
}

bool EditorLayer::IsEditorActionAvailable(UI::EditorShortcutAction action) const
{
	const bool projectLoaded = HasProjectLoaded();
	const bool editMode = m_SceneState == SceneState::Edit;
	const bool hasSelection = (bool)m_SceneHierarchyPanel.GetSelectedEntity();

	switch (action)
	{
	case UI::EditorShortcutAction::OpenProject:
	case UI::EditorShortcutAction::OpenCommandPalette:
		return true;
	case UI::EditorShortcutAction::OpenSettings:
		return projectLoaded;
	case UI::EditorShortcutAction::NewScene:
	case UI::EditorShortcutAction::SaveScene:
	case UI::EditorShortcutAction::SaveSceneAs:
	case UI::EditorShortcutAction::SaveProject:
	case UI::EditorShortcutAction::CloseScene:
	case UI::EditorShortcutAction::ReloadScripts:
	case UI::EditorShortcutAction::SelectAll:
		return projectLoaded && editMode;
	case UI::EditorShortcutAction::DuplicateEntity:
	case UI::EditorShortcutAction::DeleteEntity:
	case UI::EditorShortcutAction::Copy:
	case UI::EditorShortcutAction::Cut:
		return projectLoaded && editMode && hasSelection;
	case UI::EditorShortcutAction::Paste:
		return projectLoaded && editMode && m_HistoryManager.HasClipboard();
	case UI::EditorShortcutAction::Undo:
		return projectLoaded && editMode && m_HistoryManager.CanUndo();
	case UI::EditorShortcutAction::Redo:
		return projectLoaded && editMode && m_HistoryManager.CanRedo();
	case UI::EditorShortcutAction::Play:
		return projectLoaded && m_SceneState != SceneState::Simulate;
	case UI::EditorShortcutAction::Simulate:
		return projectLoaded && m_SceneState != SceneState::Play;
	case UI::EditorShortcutAction::Stop:
		return m_SceneState == SceneState::Play || m_SceneState == SceneState::Simulate;
	case UI::EditorShortcutAction::Pause:
		return projectLoaded && m_SceneState != SceneState::Edit;
	case UI::EditorShortcutAction::GizmoNone:
	case UI::EditorShortcutAction::GizmoTranslate:
	case UI::EditorShortcutAction::GizmoRotate:
	case UI::EditorShortcutAction::GizmoScale:
		return projectLoaded && editMode && !m_GizmoUsing;
	default:
		return false;
	}
}

void EditorLayer::RebuildEditorPanelRegistry()
{
	m_PanelManager.Clear();
	if (m_StatisticsPanelAdapter)
		m_PanelManager.AddPanel(*m_StatisticsPanelAdapter);
	m_PanelManager.AddPanel(m_SceneHierarchyPanel);
	m_PanelManager.AddPanel(m_AnimationEditorPanel);
	m_PanelManager.AddPanel(m_AssetEditorPanel);
	if (m_ConsolePanelAdapter)
		m_PanelManager.AddPanel(*m_ConsolePanelAdapter);
	if (m_ContentBrowserPanel)
		m_PanelManager.AddPanel(*m_ContentBrowserPanel);
}

bool EditorLayer::OnMouseButtonPressed(MouseButtonPressedEvent& event)
{
    if (event.GetMouseButton() == Mouse::ButtonLeft)
    {
        if (m_ViewportHovered && !m_GizmoHovered && !m_GizmoUsing && !Input::IsKeyDown(Key::LeftAlt) && Application::Get().GetImGuiLayer()->GetActiveWidgetID() == 0)
		{
			bool append = IsControlDown();
            m_SceneHierarchyPanel.SetSelectedEntity(m_HoveredEntity, append);
		}
    }
    return false;
}

bool EditorLayer::OnWindowDrop(WindowDropEvent& event)
{
	if (!HasProjectLoaded())
		return false;

	if (m_ContentBrowserPanel && m_ContentBrowserPanel->IsHovered())
		return m_ContentBrowserPanel->HandleExternalDrop(event.GetPaths());

	bool handled = false;
	for (const auto& path : event.GetPaths())
	{
		AssetHandle handle = m_AssetInteractionManager.ImportExternalAssetFile(path);
		if (handle != 0)
		{
			handled = true;
			if (m_ViewportHovered)
				m_AssetInteractionManager.HandleViewportAssetDrop(*this, handle);
		}
	}
	if (m_ContentBrowserPanel)
		m_ContentBrowserPanel->RefreshAssetTree();
	return handled;
}

void EditorLayer::DrawEditorGrid()
{
	if (m_ViewportSize.x <= 0.0f || m_ViewportSize.y <= 0.0f)
		return;

	const float aspectRatio = m_ViewportSize.x / m_ViewportSize.y;
	const float distance = glm::max(m_EditorCamera.GetDistance(), 1.0f);
	const float visibleHeight = distance * 1.15f;
	const float visibleWidth = visibleHeight * aspectRatio;
	const glm::vec3 center = m_EditorCamera.GetPosition() + m_EditorCamera.GetForwardDirection() * distance;

	float gridStep = 1.0f;
	const float visibleSpan = glm::max(visibleWidth, visibleHeight);
	while ((visibleSpan / gridStep) > 240.0f)
		gridStep *= 2.0f;

	const int minX = static_cast<int>(std::floor((center.x - visibleWidth * 0.5f) / gridStep)) - 2;
	const int maxX = static_cast<int>(std::ceil((center.x + visibleWidth * 0.5f) / gridStep)) + 2;
	const int minY = static_cast<int>(std::floor((center.y - visibleHeight * 0.5f) / gridStep)) - 2;
	const int maxY = static_cast<int>(std::ceil((center.y + visibleHeight * 0.5f) / gridStep)) + 2;

	const glm::vec4 gridColor{ 0.26f, 0.29f, 0.30f, 0.34f };
	const glm::vec4 majorGridColor{ 0.37f, 0.41f, 0.41f, 0.45f };
	const glm::vec4 xAxisColor{ 0.86f, 0.34f, 0.30f, 0.74f };
	const glm::vec4 yAxisColor{ 0.30f, 0.66f, 0.46f, 0.74f };
	const float minZ = -0.02f;
	auto isMajorGridLine = [](float value)
	{
		return std::fmod(std::abs(value), 10.0f) < 0.0001f;
	};

	Renderer2D::BeginScene(m_EditorCamera);
	Renderer2D::SetLineWidth(1.0f);

	for (int x = minX; x <= maxX; ++x)
	{
		const float worldX = static_cast<float>(x) * gridStep;
		const bool isAxis = std::abs(worldX) < 0.0001f;
		const bool isMajor = isMajorGridLine(worldX);
		const glm::vec4& color = isAxis ? yAxisColor : (isMajor ? majorGridColor : gridColor);
		Renderer2D::DrawLine(
			{ worldX, static_cast<float>(minY) * gridStep, minZ },
			{ worldX, static_cast<float>(maxY) * gridStep, minZ },
			color);
	}

	for (int y = minY; y <= maxY; ++y)
	{
		const float worldY = static_cast<float>(y) * gridStep;
		const bool isAxis = std::abs(worldY) < 0.0001f;
		const bool isMajor = isMajorGridLine(worldY);
		const glm::vec4& color = isAxis ? xAxisColor : (isMajor ? majorGridColor : gridColor);
		Renderer2D::DrawLine(
			{ static_cast<float>(minX) * gridStep, worldY, minZ },
			{ static_cast<float>(maxX) * gridStep, worldY, minZ },
			color);
	}

	Renderer2D::EndScene();
}

void EditorLayer::OnOverlayRender()
{
	WHP_PROFILE_FUNCTION();
	if (m_SceneState == SceneState::Play)
	{
		Entity cam = m_ActiveScene->GetPrimaryCameraEntity();
		if (!cam)
			return;
		Renderer2D::BeginScene(cam.GetComponent<CameraComponent>().m_Camera, cam.GetComponent<TransformComponent>().GetTransform());
	}
	else
	{
		Renderer2D::BeginScene(m_EditorCamera);
	}

	if (m_UISettings.GetShowPhysicsColliders())
	{
		// Box Colliders
		{
			auto view = m_ActiveScene->GetAllEntitiesWith<TransformComponent, BoxCollider2DComponent>();
			for (auto Entity : view)
			{
				auto [tc, bc2d] = view.get<TransformComponent, BoxCollider2DComponent>(Entity);

				glm::vec3 translation = tc.m_Translation + glm::vec3(bc2d.m_Offset, 0.001f);
				glm::vec3 scale = tc.m_Scale * glm::vec3(bc2d.m_Size * 2.0f, 1.0f);

				glm::mat4 transform = glm::translate(glm::mat4(1.0f), tc.m_Translation)
					* glm::rotate(glm::mat4(1.0f), tc.m_Rotation.z, glm::vec3(0.0f, 0.0f, 1.0f))
					* glm::translate(glm::mat4(1.0f), glm::vec3(bc2d.m_Offset, 0.001f))
					* glm::scale(glm::mat4(1.0f), scale);

				Renderer2D::DrawRect(transform, glm::vec4(0, 1, 0, 1));
			}
		}

		// Circle Colliders
		{
			auto view = m_ActiveScene->GetAllEntitiesWith<TransformComponent, CircleCollider2DComponent>();
			for (auto Entity : view)
			{
				auto [tc, cc2d] = view.get<TransformComponent, CircleCollider2DComponent>(Entity);

				glm::vec3 translation = tc.m_Translation + glm::vec3(cc2d.m_Offset, 0.001f);
				glm::vec3 scale = tc.m_Scale * glm::vec3(cc2d.m_Radius * 2.0f);

				glm::mat4 transform = glm::translate(glm::mat4(1.0f), translation)
					* glm::scale(glm::mat4(1.0f), scale);

				Renderer2D::DrawCircle(transform, glm::vec4(0, 1, 0, 1), 0.02f);
			}
		}
	}

	for (Entity selectedEntity : m_SceneHierarchyPanel.GetSelectedEntities())
	{
		TransformComponent transform = selectedEntity.GetComponent<TransformComponent>();
		if (selectedEntity.HasComponent<TextComponent>() && !selectedEntity.HasComponent<SpriteRendererComponent>() && !selectedEntity.HasComponent<CircleRendererComponent>())
		{
			selectedEntity.GetComponent<TextComponent>();
		}
		else
			Renderer2D::DrawRect(transform.GetTransform(), glm::vec4(0.9f, 0.4f, 0.1f, 1.0f));
	}

	Renderer2D::EndScene();
}

bool EditorLayer::HasProjectLoaded() const
{
	return Project::GetActive() != nullptr;
}

bool EditorLayer::NewProject(const UI::ProjectCreateSettings& settings)
{
	const std::string projectName = SanitizeProjectToken(settings.m_Name, "Untitled");
	const std::string projectFolderName = SanitizePathToken(projectName, "Untitled");
	const std::string initialSceneName = SanitizePathToken(settings.m_InitialSceneName, "Main");
	if (settings.m_Location.empty())
		return false;

	std::filesystem::path projectDirectory = settings.m_Location / projectFolderName;
	std::filesystem::path projectPath = projectDirectory / (projectFolderName + FileExtensions::Project);
	std::error_code error;
	if (std::filesystem::exists(projectPath, error))
	{
		WHP_EDITOR_WARN(std::string("[Whip Hub] Project file already exists: ") + projectPath.string());
		return false;
	}

	if (!CreateDirectoryChecked(projectDirectory / "Assets" / "Scenes", "Project scenes directory") ||
		!CreateDirectoryChecked(projectDirectory / "Assets" / "Scripts" / "Source", "script source directory") ||
		!CreateDirectoryChecked(projectDirectory / "Assets" / "Scripts" / "Binaries", "script binaries directory") ||
		!CreateDirectoryChecked(projectDirectory / "Assets" / "Animations", "animations directory") ||
		!CreateDirectoryChecked(projectDirectory / "Assets" / "Audios", "audio directory") ||
		!CreateDirectoryChecked(projectDirectory / "Assets" / "fonts", "font directory") ||
		!CreateDirectoryChecked(projectDirectory / "Assets" / "textures", "texture directory"))
	{
		return false;
	}

	Ref<Project> newProject = Project::NewProject();
	Project::SetActiveProjectPath(projectPath);

	ProjectConfig& config = newProject->GetConfig();
	config.m_Name = projectName;
	config.m_AssetDirectory = "Assets";
	config.m_AssetRegistryPath = FileExtensions::AssetRegistryFilename;
	config.m_ScriptModulePath = std::filesystem::path("Scripts") / "Binaries" / (projectFolderName + ".dll");
	config.m_StartScene = 0;

	if (!EditorScriptManager::WriteProjectFiles(projectDirectory, projectFolderName))
	{
		Project::SetActive(nullptr);
		return false;
	}

	std::filesystem::path startSceneRelativePath;
	AssetHandle startSceneHandle = 0;
	if (settings.m_CreateStartScene)
	{
		startSceneHandle = AssetHandle{};
		config.m_StartScene = startSceneHandle;
		startSceneRelativePath = std::filesystem::path("Scenes") / (initialSceneName + FileExtensions::Scene);

	Ref<Scene> startScene = MakeRef<Scene>(startSceneHandle);
		if (settings.m_TemplateIndex == 1 || settings.m_TemplateIndex == 2)
		{
			Entity camera = startScene->CreateEntity("Main Camera");
			camera.AddComponent<CameraComponent>();
			camera.GetComponent<TransformComponent>().m_Translation = { 0.0f, 0.0f, 8.0f };

			Entity sprite = startScene->CreateEntity(settings.m_TemplateIndex == 2 ? "Starter Entity" : "Sprite");
			sprite.AddComponent<SpriteRendererComponent>(glm::vec4{ 0.86f, 0.58f, 0.28f, 1.0f });
			if (settings.m_TemplateIndex == 2)
				sprite.AddComponent<ScriptComponent>().m_ClassName = projectFolderName + ".StarterEntity";
		}

		SceneImporter::SaveScene(startScene, startSceneRelativePath);
	}

	if (!Project::SaveActive(projectPath))
	{
		Project::SetActive(nullptr);
		return false;
	}

	std::ofstream registry(projectPath.parent_path() / config.m_AssetDirectory / config.m_AssetRegistryPath, std::ios::trunc);
	if (!registry)
	{
		WHP_EDITOR_ERROR("[Whip Hub] Could not write Asset registry.");
		Project::SetActive(nullptr);
		return false;
	}

	if (settings.m_CreateStartScene)
	{
		registry << "AssetRegistry:\n";
		registry << "  - handle: " << (uint64_t)startSceneHandle << '\n';
		registry << "    filepath: " << startSceneRelativePath.generic_string() << '\n';
		registry << "    type: scene\n";
	}
	else
	{
		registry << "AssetRegistry: []\n";
	}
	registry.close();

	Project::SetActive(nullptr);
	if (!settings.m_OpenAfterCreate)
	{
		m_ProjectManager.AddRecentProject(*this, projectPath);
		return true;
	}
	return OpenProject(projectPath);
}

bool EditorLayer::OpenProject()
{
	std::string filepath = FileDialogs::OpenFile("Whip Project (*.wproj)\0*.wproj\0");
	if (filepath.empty())
		return false;
	return OpenProject(filepath);
}

bool EditorLayer::OpenProject(const std::filesystem::path& path)
{
	if (path.empty())
		return false;

	std::filesystem::path projectPath = NormalizeProjectListPath(path);
	std::error_code error;
	if (!std::filesystem::exists(projectPath, error) || !FileExtensions::IsProjectExtension(projectPath))
	{
		WHP_EDITOR_WARN(std::string("[Project] Project file is missing or invalid: ") + projectPath.string());
		m_ProjectManager.GetLoader().SetStatus("Project file is missing or invalid.");
		return false;
	}

	if (HasProjectLoaded() && PathsMatchForRecentProject(Project::GetActive()->GetProjectPath(), projectPath))
	{
		WHP_EDITOR_INFO(std::string("[Project] Project is already open: ") + projectPath.string());
		m_ProjectManager.AddRecentProject(*this, projectPath);
		m_ProjectManager.GetLoader().SetLoaded(true);
		m_ProjectManager.GetLoader().SetStatus("Project already open.");
		return true;
	}

	WHP_EDITOR_INFO(std::string("[Project] Opening Project: ") + projectPath.string());
	if (HasProjectLoaded())
	{
		WHP_EDITOR_INFO("[Project] Unloading current Project before opening a new one.");
		ResetEditorProjectState();
	}

	if (Project::Load(projectPath))
	{
		WHP_EDITOR_INFO("[Project] Project file loaded.");
		m_ProjectManager.MigrateProjectNativeFileExtensions();
		WHP_EDITOR_INFO("[Project] Native file extension migration complete.");
		const bool scriptBuildSucceeded = m_ScriptManager.BuildProjectScripts();
		if (!scriptBuildSucceeded)
			WHP_EDITOR_WARN("[Script Build] Project opened, but script build failed.");
		WHP_EDITOR_INFO("[Project] Script build step complete.");
		ScriptEngine::Init();
		WHP_EDITOR_INFO("[Project] Script engine initialized.");
		m_ScriptManager.StartSourceWatcher();
		AssetHandle startScene = (Project::GetActive()->GetConfig().m_StartScene);
		if (startScene && Project::GetActive()->GetEditorAssetManager()->IsAssetHandleValid(startScene))
		{
			const std::filesystem::path startScenePath = Project::GetActive()->GetEditorAssetManager()->GetFilepath(startScene);
			if (std::filesystem::exists(Project::GetActiveAssetDirectory() / startScenePath))
				OpenScene(startScene);
			else
			{
				WHP_EDITOR_WARN("[Project] Start scene file is missing. Resetting Project start scene.");
				Project::GetActive()->GetConfig().m_StartScene = 0;
				Project::SaveActive();
			}
		}
		else if (startScene)
		{
			WHP_EDITOR_WARN("[Project] Start scene is missing. Resetting Project start scene.");
			Project::GetActive()->GetConfig().m_StartScene = 0;
			Project::SaveActive();
		}
		else
		{
			NewScene();
		}
		m_ContentBrowserPanel = MakeScope<ContentBrowserPanel>(Project::GetActive());
		m_ContentBrowserPanel->SetAssetOpenCallback([this](AssetHandle handle) { return m_AssetInteractionManager.HandleContentBrowserAssetOpen(*this, handle); });
		m_ContentBrowserPanel->SetAssetInspectCallback([this](AssetHandle handle) { return m_AssetInteractionManager.HandleContentBrowserAssetInspect(*this, handle); });
		m_ProjectManager.ApplyPreferencesToContentBrowser(*this);
		m_ProjectManager.AddRecentProject(*this, projectPath);
		m_ProjectManager.GetLoader().SetLoaded(true);
		m_ProjectManager.GetLoader().SetStatus(scriptBuildSucceeded ? "Project opened." : "Project opened, script build failed.");
		WHP_EDITOR_INFO("[Project] Project open complete.");
		return true;
	}
	ResetEditorProjectState();
	WHP_EDITOR_WARN(std::string("[Project] Project load failed: ") + projectPath.string());
	m_ProjectManager.GetLoader().SetStatus("Project could not be opened.");
	return false;
}

void EditorLayer::ResetEditorProjectState()
{
	WriteSceneRecoverySnapshot("Project switch");
	m_ScriptManager.StopSourceWatcher();
	if (m_SceneState != SceneState::Edit)
		OnSceneStop();

	ScriptEngine::ClearRuntimeSceneTransitionRequest();
	m_ScriptManager.Reset();

	m_ContentBrowserPanel.reset();
	m_SceneHierarchyPanel.SetContext({});
	m_EditorScene = MakeRef<Scene>();
	m_ActiveScene = m_EditorScene;
	m_EditorScenePath.clear();
	m_HistoryManager.ClearSceneHistory();
	MarkSceneClean();
	Project::SetActive(nullptr);
	m_ProjectManager.GetLoader().SetLoaded(false);
}

void EditorLayer::NewScene()
{
	if (!HasProjectLoaded())
		return;

    m_ActiveScene = MakeRef<Scene>();
	m_SceneHierarchyPanel.SetContext(m_ActiveScene);
	m_EditorScenePath = std::filesystem::path();
	m_HistoryManager.ClearSceneHistory();
	MarkSceneClean();
}

void EditorLayer::OpenScene(AssetHandle handle)
{
	if (!HasProjectLoaded())
		return;

	if (m_SceneState != SceneState::Edit)
		OnSceneStop();

	if (!Project::GetActive()->GetEditorAssetManager()->IsAssetHandleValid(handle))
	{
		WHP_EDITOR_WARN("[Scene] Failed to open scene. Asset handle is not registered.");
		return;
	}
	const std::filesystem::path scenePath = Project::GetActive()->GetEditorAssetManager()->GetFilepath(handle);
	if (!std::filesystem::exists(Project::GetActiveAssetDirectory() / scenePath))
	{
		WHP_EDITOR_WARN(std::string("[Scene] Failed to open scene. File is missing: ") + scenePath.string());
		return;
	}

	Ref<Scene> readOnlyScene = AssetManager::GetAsset<Scene>(handle);
	if (!readOnlyScene)
	{
		WHP_EDITOR_WARN("[Scene] Failed to open scene. Asset is missing or failed to import.");
		return;
	}
	Ref<Scene> NewScene = Scene::Copy(readOnlyScene);

	m_EditorScene = NewScene;
	m_SceneHierarchyPanel.SetContext(m_EditorScene);

	m_ActiveScene = m_EditorScene;
	m_EditorScenePath = scenePath;
	m_HistoryManager.ClearSceneHistory();
	MarkSceneClean();
}

void EditorLayer::CloseScene()
{
	if (m_SceneState != SceneState::Edit)
		OnSceneStop();
	Ref<Scene> NewScene = MakeRef<Scene>();
	m_EditorScene = NewScene;
	m_EditorScene->OnViewportResize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
	m_ActiveScene = m_EditorScene;
	m_EditorScenePath.clear();
	m_SceneHierarchyPanel.SetContext({});
	m_HistoryManager.ClearSceneHistory();
	MarkSceneClean();
}

void EditorLayer::SaveScene()
{
	if (!HasProjectLoaded())
		return;

	if (!m_EditorScenePath.empty())
	{
		SerializeScene(m_ActiveScene, m_EditorScenePath);
		MarkSceneClean();
	}
	else
		SaveSceneAs();
}

void EditorLayer::SaveSceneAs()
{
	if (!HasProjectLoaded())
		return;

	const std::filesystem::path scenesDirectory = Project::GetActiveAssetDirectory() / "Scenes";
	std::error_code error;
	std::filesystem::create_directories(scenesDirectory, error);

    std::string filepath = FileDialogs::SaveFile("Whip Scene (*.wscene)\0*.wscene\0", scenesDirectory.string().c_str());
	if (filepath.empty())
		return;

	std::filesystem::path scenePath(filepath);
	if (!FileExtensions::IsSceneExtension(scenePath))
		scenePath.replace_extension(FileExtensions::Scene);
	else if (FileExtensions::IsLegacySceneExtension(scenePath))
		scenePath.replace_extension(FileExtensions::Scene);

	SerializeScene(m_ActiveScene, scenePath);

	const std::filesystem::path assetDirectory = Project::GetActiveAssetDirectory();
	if (PathIsOrIsUnder(scenePath, assetDirectory))
	{
		error.clear();
		const std::filesystem::path RelativePath = std::filesystem::relative(scenePath, assetDirectory, error).lexically_normal();
		if (!error)
		{
			m_EditorScenePath = RelativePath;
			AssetHandle handle = Project::GetActive()->GetEditorAssetManager()->GetHandleFromFilepath(RelativePath);
			if (handle == 0)
				handle = Project::GetActive()->GetEditorAssetManager()->ImportAsset(RelativePath);
			if (handle != 0)
			{
				m_ActiveScene->m_Handle = handle;
				if (m_EditorScene)
					m_EditorScene->m_Handle = handle;
			}
		}
		else
		{
			m_EditorScenePath = scenePath;
		}
	}
	else
	{
		m_EditorScenePath = scenePath;
	}

	if (m_ContentBrowserPanel)
		m_ContentBrowserPanel->RefreshAssetTree();
	MarkSceneClean();
}

void EditorLayer::MarkSceneDirty()
{
	if (m_SceneState == SceneState::Edit && m_EditorScene)
		m_SceneManager.MarkDirty();
}

void EditorLayer::MarkSceneClean()
{
	m_SceneManager.MarkClean();
}

std::filesystem::path EditorLayer::GetSceneRecoveryPath() const
{
	return m_SceneManager.GetRecoveryPath();
}

void EditorLayer::WriteSceneRecoverySnapshot(const char* reason)
{
	if (!HasProjectLoaded() || !m_EditorScene || !m_SceneDirty || m_SceneState != SceneState::Edit)
		return;

	const std::filesystem::path recoveryPath = GetSceneRecoveryPath();
	if (recoveryPath.empty())
		return;

	std::error_code error;
	std::filesystem::create_directories(recoveryPath.parent_path(), error);
	if (error)
	{
		WHP_EDITOR_WARN(std::string("[Scene Recovery] Could not create recovery directory: ") + error.message());
		return;
	}

	SceneImporter::SaveScene(m_EditorScene, recoveryPath);
	m_LastSceneRecoverySnapshot = std::chrono::steady_clock::now();
	WHP_EDITOR_INFO(std::string("[Scene Recovery] Snapshot written (") + reason + "): " + recoveryPath.string());
}

void EditorLayer::SaveEntityTemplate(Entity entityIn)
{
	if (!HasProjectLoaded() || !entityIn)
		return;

	const std::filesystem::path templatesDirectory = Project::GetActiveAssetDirectory() / "EntityTemplates";
	std::error_code error;
	std::filesystem::create_directories(templatesDirectory, error);

	std::string filepath = FileDialogs::SaveFile("Whip Entity Template (*.went)\0*.went\0", templatesDirectory.string().c_str());
	if (filepath.empty())
		return;

	std::filesystem::path templatePath(filepath);
	if (!FileExtensions::IsEntityTemplateExtension(templatePath))
		templatePath.replace_extension(FileExtensions::EntityTemplate);

	SceneSerializer serializer(m_EditorScene);
	if (!serializer.SerializeEntityTemplate(entityIn, templatePath))
	{
		WHP_EDITOR_ERROR(std::string("[Entity Template] Could not save template: ") + templatePath.string());
		return;
	}

	const std::filesystem::path assetDirectory = Project::GetActiveAssetDirectory();
	if (PathIsOrIsUnder(templatePath, assetDirectory))
	{
		error.clear();
		const std::filesystem::path RelativePath = std::filesystem::relative(templatePath, assetDirectory, error).lexically_normal();
		if (!error)
			Project::GetActive()->GetEditorAssetManager()->ImportAsset(RelativePath);
	}

	if (m_ContentBrowserPanel)
		m_ContentBrowserPanel->RefreshAssetTree();

	WHP_EDITOR_INFO(std::string("[Entity Template] Saved ") + templatePath.string());
}

bool EditorLayer::InstantiateEntityTemplate(AssetHandle handle)
{
	if (!HasProjectLoaded() || !m_EditorScene)
		return false;

	Ref<Project> activeProject = Project::GetActive();
	if (!activeProject->GetEditorAssetManager()->IsAssetHandleValid(handle) ||
		activeProject->GetEditorAssetManager()->GetAssetType(handle) != AssetType::Entity)
	{
		return false;
	}

	if (m_SceneState != SceneState::Edit)
	{
		WHP_EDITOR_WARN("[Entity Template] Templates can only be instantiated while editing.");
		return false;
	}

	const std::filesystem::path templatePath = Project::GetActiveAssetDirectory() / activeProject->GetEditorAssetManager()->GetFilepath(handle);
	m_HistoryManager.CaptureSceneHistory(*this);
	SceneSerializer serializer(m_EditorScene);
	Entity instance = serializer.DeserializeEntityTemplate(templatePath, handle);
	if (!instance)
	{
		WHP_EDITOR_ERROR(std::string("[Entity Template] Could not instantiate template: ") + templatePath.string());
		return false;
	}

	m_SceneHierarchyPanel.SetSelectedEntity(instance);
	WHP_EDITOR_INFO(std::string("[Entity Template] Instantiated ") + templatePath.string());
	return true;
}

Entity EditorLayer::FindPrefabRoot(Entity entityIn) const
{
	if (!entityIn || !entityIn.HasComponent<PrefabComponent>())
		return {};

	const AssetHandle source = entityIn.GetComponent<PrefabComponent>().m_Source;
	Entity current = entityIn;
	while (current && current.HasComponent<HierarchyComponent>())
	{
		if (current.HasComponent<PrefabComponent>())
		{
			const auto& prefab = current.GetComponent<PrefabComponent>();
			if (prefab.m_Source == source && prefab.m_Root)
				return current;
		}

		const auto& hierarchy = current.GetComponent<HierarchyComponent>();
		if (hierarchy.m_Parent == 0)
			break;

		current = m_EditorScene ? m_EditorScene->FindEntityByUUID(hierarchy.m_Parent) : Entity{};
	}

	return entityIn.GetComponent<PrefabComponent>().m_Root ? entityIn : Entity{};
}

void EditorLayer::RemovePrefabLinksRecursive(Entity entityIn)
{
	if (!entityIn)
		return;

	std::vector<UUID> children;
	if (entityIn.HasComponent<HierarchyComponent>())
		children = entityIn.GetComponent<HierarchyComponent>().m_Children;

	if (entityIn.HasComponent<PrefabComponent>())
		entityIn.RemoveComponent<PrefabComponent>();

	for (UUID childId : children)
	{
		Entity child = m_EditorScene ? m_EditorScene->FindEntityByUUID(childId) : Entity{};
		if (child)
			RemovePrefabLinksRecursive(child);
	}
}

void EditorLayer::ApplyEntityTemplate(Entity entityIn)
{
	if (!HasProjectLoaded() || !m_EditorScene)
		return;

	Entity root = FindPrefabRoot(entityIn);
	if (!root || !root.HasComponent<PrefabComponent>())
	{
		WHP_EDITOR_WARN("[Entity Template] Apply failed. Select a template instance root or child.");
		return;
	}

	Ref<Project> activeProject = Project::GetActive();
	AssetHandle handle = root.GetComponent<PrefabComponent>().m_Source;
	if (!activeProject->GetEditorAssetManager()->IsAssetHandleValid(handle) ||
		activeProject->GetEditorAssetManager()->GetAssetType(handle) != AssetType::Entity)
	{
		WHP_EDITOR_WARN("[Entity Template] Apply failed. The source template Asset is missing.");
		return;
	}

	const std::filesystem::path templatePath = Project::GetActiveAssetDirectory() / activeProject->GetEditorAssetManager()->GetFilepath(handle);
	std::error_code error;
	if (!std::filesystem::exists(templatePath, error))
	{
		WHP_EDITOR_WARN(std::string("[Entity Template] Apply failed. File is missing: ") + templatePath.string());
		return;
	}

	SceneSerializer serializer(m_EditorScene);
	if (!serializer.SerializeEntityTemplate(root, templatePath))
	{
		WHP_EDITOR_ERROR(std::string("[Entity Template] Could not apply instance to template: ") + templatePath.string());
		return;
	}

	activeProject->GetEditorAssetManager()->UnloadAsset(handle);
	if (m_ContentBrowserPanel)
		m_ContentBrowserPanel->RefreshAssetTree();

	WHP_EDITOR_INFO(std::string("[Entity Template] Applied instance to ") + templatePath.string());
}

void EditorLayer::RevertEntityTemplate(Entity entityIn)
{
	if (!HasProjectLoaded() || !m_EditorScene)
		return;

	Entity root = FindPrefabRoot(entityIn);
	if (!root || !root.HasComponent<PrefabComponent>())
	{
		WHP_EDITOR_WARN("[Entity Template] Revert failed. Select a template instance root or child.");
		return;
	}

	Ref<Project> activeProject = Project::GetActive();
	AssetHandle handle = root.GetComponent<PrefabComponent>().m_Source;
	if (!activeProject->GetEditorAssetManager()->IsAssetHandleValid(handle) ||
		activeProject->GetEditorAssetManager()->GetAssetType(handle) != AssetType::Entity)
	{
		WHP_EDITOR_WARN("[Entity Template] Revert failed. The source template Asset is missing.");
		return;
	}

	const std::filesystem::path templatePath = Project::GetActiveAssetDirectory() / activeProject->GetEditorAssetManager()->GetFilepath(handle);
	std::error_code error;
	if (!std::filesystem::exists(templatePath, error))
	{
		WHP_EDITOR_WARN(std::string("[Entity Template] Revert failed. File is missing: ") + templatePath.string());
		return;
	}

	{
		Ref<Scene> validationScene = MakeRef<Scene>();
		SceneSerializer validator(validationScene);
		if (!validator.DeserializeEntityTemplate(templatePath, handle))
		{
			WHP_EDITOR_ERROR(std::string("[Entity Template] Revert failed. Could not read template: ") + templatePath.string());
			return;
		}
	}

	UUID parentId = 0;
	size_t childIndex = 0;
	bool hadChildIndex = false;
	if (root.HasComponent<HierarchyComponent>())
	{
		const auto& hierarchy = root.GetComponent<HierarchyComponent>();
		parentId = hierarchy.m_Parent;
		if (parentId != 0)
		{
			Entity parent = m_EditorScene->FindEntityByUUID(parentId);
			if (parent && parent.HasComponent<HierarchyComponent>())
			{
				const auto& siblings = parent.GetComponent<HierarchyComponent>().m_Children;
				auto siblingIt = std::find(siblings.begin(), siblings.end(), root.GetUUID());
				if (siblingIt != siblings.end())
				{
					childIndex = static_cast<size_t>(std::distance(siblings.begin(), siblingIt));
					hadChildIndex = true;
				}
			}
		}
	}

	TransformComponent preservedTransform{};
	if (root.HasComponent<TransformComponent>())
		preservedTransform = root.GetComponent<TransformComponent>();

	m_HistoryManager.CaptureSceneHistory(*this);
	m_EditorScene->DestroyEntity(root);

	SceneSerializer serializer(m_EditorScene);
	Entity reverted = serializer.DeserializeEntityTemplate(templatePath, handle);
	if (!reverted)
	{
		WHP_EDITOR_ERROR(std::string("[Entity Template] Revert failed after validation: ") + templatePath.string());
		return;
	}

	if (reverted.HasComponent<TransformComponent>())
		reverted.GetComponent<TransformComponent>() = preservedTransform;

	if (parentId != 0 && reverted.HasComponent<HierarchyComponent>())
	{
		Entity parent = m_EditorScene->FindEntityByUUID(parentId);
		if (parent && parent.HasComponent<HierarchyComponent>())
		{
			auto& hierarchy = reverted.GetComponent<HierarchyComponent>();
			hierarchy.m_Parent = parentId;

			auto& siblings = parent.GetComponent<HierarchyComponent>().m_Children;
			siblings.erase(std::remove(siblings.begin(), siblings.end(), reverted.GetUUID()), siblings.end());
			size_t insertIndex = hadChildIndex ? std::min(childIndex, siblings.size()) : siblings.size();
			siblings.insert(siblings.begin() + static_cast<std::vector<UUID>::difference_type>(insertIndex), reverted.GetUUID());
		}
	}

	m_SceneHierarchyPanel.SetSelectedEntity(reverted);
	WHP_EDITOR_INFO(std::string("[Entity Template] Reverted instance from ") + templatePath.string());
}

void EditorLayer::UnpackEntityTemplate(Entity entityIn)
{
	if (!HasProjectLoaded() || !m_EditorScene)
		return;

	Entity root = FindPrefabRoot(entityIn);
	if (!root)
	{
		WHP_EDITOR_WARN("[Entity Template] Unpack failed. Select a template instance root or child.");
		return;
	}

	m_HistoryManager.CaptureSceneHistory(*this);
	RemovePrefabLinksRecursive(root);
	m_SceneHierarchyPanel.SetSelectedEntity(root);
	WHP_EDITOR_INFO(std::string("[Entity Template] Unpacked instance ") + root.GetName());
}

void EditorLayer::ProcessRuntimeSceneTransition()
{
	if (m_SceneState != SceneState::Play && m_SceneState != SceneState::Simulate)
	{
		ScriptEngine::ClearRuntimeSceneTransitionRequest();
		return;
	}

	const RuntimeSceneTransitionRequest request = ScriptEngine::ConsumeRuntimeSceneTransitionRequest();
	switch (request.m_Type)
	{
	case RuntimeSceneTransitionType::Load:
	case RuntimeSceneTransitionType::Reload:
		LoadRuntimeScene(request.m_SceneHandle);
		break;
	case RuntimeSceneTransitionType::Unload:
		UnloadRuntimeScene();
		break;
	default:
		break;
	}
}

bool EditorLayer::LoadRuntimeScene(AssetHandle handle)
{
	if (!HasProjectLoaded() || handle == 0)
		return false;

	Ref<Project> activeProject = Project::GetActive();
	if (!activeProject->GetRuntimeAssetManager()->IsAssetHandleValid(handle) ||
		activeProject->GetRuntimeAssetManager()->GetAssetType(handle) != AssetType::Scene)
	{
		WHP_EDITOR_WARN("[Scene Manager] Runtime scene load failed. Invalid scene handle.");
		return false;
	}

	Ref<Scene> sourceScene = AssetManager::GetAsset<Scene>(handle);
	if (!sourceScene)
	{
		WHP_EDITOR_WARN("[Scene Manager] Runtime scene load failed. Scene Asset could not be loaded.");
		return false;
	}

	StopActiveRuntimeSceneForTransition();
	m_ActiveScene = Scene::Copy(sourceScene);
	m_ActiveScene->OnViewportResize(static_cast<uint32_t>(m_ViewportSize.x), static_cast<uint32_t>(m_ViewportSize.y));
	m_SceneHierarchyPanel.SetContext(m_ActiveScene);
	m_SceneHierarchyPanel.SetSelectedEntity({});
	StartActiveRuntimeSceneForTransition(handle);
	WHP_EDITOR_INFO(std::string("[Scene Manager] Runtime scene loaded: ") + activeProject->GetRuntimeAssetManager()->GetFilepath(handle).generic_string());
	return true;
}

bool EditorLayer::UnloadRuntimeScene()
{
	if (m_SceneState != SceneState::Play && m_SceneState != SceneState::Simulate)
		return false;

	StopActiveRuntimeSceneForTransition();
	m_ActiveScene = MakeRef<Scene>();
	m_ActiveScene->OnViewportResize(static_cast<uint32_t>(m_ViewportSize.x), static_cast<uint32_t>(m_ViewportSize.y));
	m_SceneHierarchyPanel.SetContext(m_ActiveScene);
	m_SceneHierarchyPanel.SetSelectedEntity({});
	StartActiveRuntimeSceneForTransition(0);
	WHP_EDITOR_INFO("[Scene Manager] Runtime scene unloaded.");
	return true;
}

void EditorLayer::StopActiveRuntimeSceneForTransition()
{
	m_SceneManager.StopActiveRuntimeSceneForTransition();
}

void EditorLayer::StartActiveRuntimeSceneForTransition(AssetHandle handle)
{
	m_SceneManager.StartActiveRuntimeSceneForTransition(handle);
}

void EditorLayer::SerializeScene(Ref<Scene> sceneIn, const std::filesystem::path& path)
{
	SceneImporter::SaveScene(sceneIn, path);

	if (!HasProjectLoaded() || !sceneIn)
		return;

	const std::filesystem::path assetDirectory = Project::GetActiveAssetDirectory();
	std::filesystem::path scenePath = path;
	if (!scenePath.is_absolute())
		scenePath = assetDirectory / scenePath;
	scenePath = scenePath.lexically_normal();

	if (!PathIsOrIsUnder(scenePath, assetDirectory))
		return;

	std::error_code error;
	const std::filesystem::path RelativePath = std::filesystem::relative(scenePath, assetDirectory, error).lexically_normal();
	if (error || RelativePath.empty() || !FileExtensions::IsSceneExtension(RelativePath))
		return;

	Ref<EditorAssetManager> EditorAssetManager = Project::GetActive()->GetEditorAssetManager();
	if (!EditorAssetManager)
		return;

	AssetHandle handle = EditorAssetManager->GetHandleFromFilepath(RelativePath);
	if (handle == 0)
		handle = EditorAssetManager->ImportAsset(RelativePath);

	if (handle == 0)
		return;

	sceneIn->m_Handle = handle;
	if (m_EditorScene)
		m_EditorScene->m_Handle = handle;
	if (m_ActiveScene)
		m_ActiveScene->m_Handle = handle;

	EditorAssetManager->SetLoadedAsset(handle, Scene::Copy(sceneIn));
}

void EditorLayer::OnScenePlay()
{
	if (!HasProjectLoaded())
		return;

	if (m_SceneState == SceneState::Simulate)
		OnSceneStop();
	WriteSceneRecoverySnapshot("Before play");
	Project::RunState(true);
	m_SceneState = SceneState::Play;
	ScriptEngine::SetFilewatcherState(false);
	m_ActiveScene = Scene::Copy(m_EditorScene);
	ScriptEngine::SetRuntimeActiveSceneHandle(m_ActiveScene->m_Handle);
	m_ActiveScene->OnRuntimeStart();
	ScriptEngine::SetRuntimeActiveSceneHandle(m_ActiveScene->m_Handle);
	m_LastSelectedEntity = m_SceneHierarchyPanel.GetSelectedEntity();
	m_SceneHierarchyPanel.SetContext(m_ActiveScene);
}

void EditorLayer::OnSceneSimulate()
{
	if (!HasProjectLoaded())
		return;

	if (m_SceneState == SceneState::Play)
		OnSceneStop();

	WriteSceneRecoverySnapshot("Before simulate");
	Project::RunState(true);
	m_SceneState = SceneState::Simulate;
	ScriptEngine::SetFilewatcherState(false);
	m_ActiveScene = Scene::Copy(m_EditorScene);
	ScriptEngine::SetRuntimeActiveSceneHandle(m_ActiveScene->m_Handle);
	m_ActiveScene->OnSimulationStart();
	ScriptEngine::SetRuntimeActiveSceneHandle(m_ActiveScene->m_Handle);
	m_LastSelectedEntity = m_SceneHierarchyPanel.GetSelectedEntity();
	m_SceneHierarchyPanel.SetContext(m_ActiveScene);
	// maybe do not ??
	if(m_LastSelectedEntity)
		m_SceneHierarchyPanel.SetSelectedEntity(m_ActiveScene->FindEntityByUUID(m_LastSelectedEntity.GetUUID()));
}

void EditorLayer::OnSceneStop()
{
	WHP_CORE_ASSERT(m_SceneState == SceneState::Play || m_SceneState == SceneState::Simulate, "invalid SceneState!");
	Project::RunState(false);
	if (m_SceneState == SceneState::Play)
		m_ActiveScene->OnRuntimeStop();
	else if (m_SceneState == SceneState::Simulate)
		m_ActiveScene->OnSimulationStop();
	m_SceneState = SceneState::Edit;
	ScriptEngine::ClearRuntimeSceneTransitionRequest();
	ScriptEngine::SetRuntimeActiveSceneHandle(0);
	ScriptEngine::SetFilewatcherState(true);
	m_ActiveScene = m_EditorScene;
	m_SceneHierarchyPanel.SetContext(m_ActiveScene);
	m_SceneHierarchyPanel.SetSelectedEntity(m_LastSelectedEntity);
}

void EditorLayer::OnScenePause()
{

}

void EditorLayer::UIToolbar()
{
	bool toolbarEnabled = (bool)m_SceneHierarchyPanel.GetContext();

	ImVec4 tintColor = ImVec4(1, 1, 1, 1);
	if (!toolbarEnabled)
		tintColor.w = 0.5f;

	bool hasPlayButton = m_SceneState == SceneState::Edit|| m_SceneState == SceneState::Play;
	bool hasSimulateButton = m_SceneState == SceneState::Edit || m_SceneState == SceneState::Simulate;
	bool hasPauseButton = m_SceneState != SceneState::Edit;
	bool isPaused = hasPauseButton && m_ActiveScene->IsPaused();
	bool hasStepButton = hasPauseButton && isPaused;

	const float buttonSize = 36.0f;
	const float iconSize = 18.0f;
	const float padding = 6.0f;
	const float spacing = 5.0f;
	const int buttonCount = (hasPlayButton ? 1 : 0) + (hasSimulateButton ? 1 : 0) + (hasPauseButton ? 1 : 0) + (hasStepButton ? 1 : 0);
	const float panelWidth = padding * 2.0f + buttonSize * buttonCount + spacing * glm::max(buttonCount - 1, 0);
	const float panelHeight = buttonSize + padding * 2.0f;

	ImVec2 viewportMin = ImVec2(m_ViewportBounds[0].x, m_ViewportBounds[0].y);
	ImVec2 viewportMax = ImVec2(m_ViewportBounds[1].x, m_ViewportBounds[1].y);
	ImVec2 panelPos = ImVec2(viewportMin.x + ((viewportMax.x - viewportMin.x) - panelWidth) * 0.5f, viewportMin.y + 12.0f);
	ImVec2 panelEnd = ImVec2(panelPos.x + panelWidth, panelPos.y + panelHeight);

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	drawList->AddRectFilled(ImVec2(panelPos.x + 2.0f, panelPos.y + 3.0f), ImVec2(panelEnd.x + 2.0f, panelEnd.y + 3.0f), IM_COL32(0, 0, 0, 76), 7.0f);
	drawList->AddRectFilled(panelPos, panelEnd, IM_COL32(24, 22, 19, 238), 7.0f);
	drawList->AddRect(panelPos, panelEnd, IM_COL32(76, 64, 48, 210), 7.0f);

	ImGui::SetCursorScreenPos(ImVec2(panelPos.x + padding, panelPos.y + padding));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(spacing, 0.0f));

	auto drawIconButton = [&](const char* id, Icon iconType, ImU32 accent, const char* tooltip) -> bool
		{
			Ref<Texture2D> iconTexture = IconManager::Get().GetIcon(iconType);
			ImGui::InvisibleButton(id, ImVec2(buttonSize, buttonSize));
			const bool clicked = ImGui::IsItemClicked() && toolbarEnabled;
			const bool hovered = ImGui::IsItemHovered();
			const bool active = ImGui::IsItemActive();
			ImVec2 min = ImGui::GetItemRectMin();
			ImVec2 max = ImGui::GetItemRectMax();
			ImU32 buttonColor = active ? ColorU32(0.33f, 0.22f, 0.12f, 0.95f) : hovered ? ColorU32(0.18f, 0.15f, 0.12f, 0.92f) : ColorU32(0.10f, 0.09f, 0.08f, 0.88f);
			drawList->AddRectFilled(min, max, buttonColor, 5.0f);
			if (hovered)
				drawList->AddRect(min, max, accent, 5.0f);

			ImVec2 center((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
			ImVec2 iconMin(center.x - iconSize * 0.5f, center.y - iconSize * 0.5f);
			ImVec2 iconMax(center.x + iconSize * 0.5f, center.y + iconSize * 0.5f);
			ImU32 tint = toolbarEnabled ? IM_COL32(240, 232, 216, 255) : IM_COL32(148, 140, 128, 190);
			drawList->AddImage(UI::ToImGuiTextureId(iconTexture->GetRendererId()), iconMin, iconMax, ImVec2(0, 1), ImVec2(1, 0), tint);
			if (hovered && tooltip)
				ImGui::SetTooltip("%s", tooltip);
			return clicked;
		};

	if(hasPlayButton)
	{
		Icon playIcon = m_SceneState == SceneState::Play ? Icon::Stop : Icon::Play;
		if (drawIconButton("##ViewportToolbarPlay", playIcon, ColorU32(0.58f, 0.70f, 0.42f, tintColor.w), m_SceneState == SceneState::Play ? "Stop" : "Play"))
		{
			if (m_SceneState == SceneState::Edit || m_SceneState == SceneState::Simulate)
				OnScenePlay();
			else if (m_SceneState == SceneState::Play)
				OnSceneStop();
		}
	}
	if(hasSimulateButton)
	{
		if(hasPlayButton)
			ImGui::SameLine();
		Icon simulateIcon = m_SceneState == SceneState::Simulate ? Icon::Stop : Icon::Simulate;
		if (drawIconButton("##ViewportToolbarSimulate", simulateIcon, ColorU32(0.66f, 0.55f, 0.42f, tintColor.w), m_SceneState == SceneState::Simulate ? "Stop simulation" : "Simulate"))
		{
			if (m_SceneState == SceneState::Edit || m_SceneState == SceneState::Play)
				OnSceneSimulate();
			else if (m_SceneState == SceneState::Simulate)
				OnSceneStop();
		}
	}
	if (hasPauseButton)
	{
		ImGui::SameLine();
		if (drawIconButton("##ViewportToolbarPause", Icon::Pause, ColorU32(0.86f, 0.64f, 0.32f, tintColor.w), isPaused ? "Resume" : "Pause"))
			m_ActiveScene->SetPaused(!isPaused);

		if (isPaused)
		{
			ImGui::SameLine();
			if (drawIconButton("##ViewportToolbarStepForward", Icon::StepForward, ColorU32(0.86f, 0.64f, 0.32f, tintColor.w), "Step"))
				m_ActiveScene->Step(m_UISettings.GetStepFrame());
		}
	}
	ImGui::PopStyleVar();

	const EditorScriptManager::Status& scriptStatus = m_ScriptManager.GetStatus();
	if (HasProjectLoaded() && !scriptStatus.m_Message.empty())
	{
		const ImVec2 textSize = ImGui::CalcTextSize(scriptStatus.m_Message.c_str());
		const float statusPaddingX = 10.0f;
		const float statusHeight = 24.0f;
		const float statusWidth = glm::min(textSize.x + statusPaddingX * 2.0f, 260.0f);
		ImVec2 statusPos(panelEnd.x + 10.0f, panelPos.y + (panelHeight - statusHeight) * 0.5f);
		if (statusPos.x + statusWidth > viewportMax.x - 10.0f)
			statusPos = ImVec2(panelPos.x - statusWidth - 10.0f, statusPos.y);

		if (statusPos.x > viewportMin.x + 10.0f)
		{
			const ImU32 statusFill = scriptStatus.m_Failure ? IM_COL32(84, 34, 32, 230) :
				scriptStatus.m_Warning ? IM_COL32(78, 58, 28, 230) : IM_COL32(34, 62, 48, 220);
			const ImU32 statusBorder = scriptStatus.m_Failure ? IM_COL32(214, 94, 84, 230) :
				scriptStatus.m_Warning ? IM_COL32(226, 174, 74, 230) : IM_COL32(112, 184, 136, 220);
			ImVec2 statusEnd(statusPos.x + statusWidth, statusPos.y + statusHeight);
			drawList->AddRectFilled(statusPos, statusEnd, statusFill, 5.0f);
			drawList->AddRect(statusPos, statusEnd, statusBorder, 5.0f);
			drawList->AddText(ImVec2(statusPos.x + statusPaddingX, statusPos.y + 4.0f), IM_COL32(238, 232, 220, 255), scriptStatus.m_Message.c_str());
			if (ImGui::IsMouseHoveringRect(statusPos, statusEnd))
				ImGui::SetTooltip("%s", scriptStatus.m_Message.c_str());
		}
	}
}

_WHIP_END
