#include <Whip-Editor/EditorLayer.h>

#include <Whip/Core/EntryPoint.h>
#include <Whip/Scene/SceneSerializer.h>
#include <Whip-Editor/UI/UIHelpers.h>
#include <Whip-Editor/UI/UIProjectLoader.h>
#include <Whip/Math/Math.h>
#include <Whip/Asset/AssetUtils.h>

#include <Whip-Editor/Helpers/IconManager.h>
#include <Whip-Editor/Panels/ConsolePanel.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <entt.hpp>
#include <ImGuizmo.h>

_WHIP_START

namespace
{
	enum class ShellWindowControl
	{
		Minimize,
		Maximize,
		Restore,
		Close
	};

	bool DrawShellWindowControlButton(const char* id, ShellWindowControl control, ImVec2 size)
	{
		ImGui::InvisibleButton(id, size);
		const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
		const bool hovered = ImGui::IsItemHovered();
		const bool active = ImGui::IsItemActive();

		const ImVec2 min = ImGui::GetItemRectMin();
		const ImVec2 max = ImGui::GetItemRectMax();
		const ImVec2 center((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
		ImDrawList* drawList = ImGui::GetWindowDrawList();

		ImU32 background = IM_COL32(255, 255, 255, hovered ? 28 : 0);
		if (control == ShellWindowControl::Close && hovered)
			background = IM_COL32(196, 58, 46, active ? 230 : 205);
		else if (active)
			background = IM_COL32(255, 255, 255, 42);

		drawList->AddRectFilled(min, max, background, 0.0f);
		const ImU32 iconColor = control == ShellWindowControl::Close && hovered ? IM_COL32(255, 244, 234, 255) : IM_COL32(226, 218, 202, 235);

		switch (control)
		{
		case ShellWindowControl::Minimize:
			drawList->AddLine(ImVec2(center.x - 5.0f, center.y + 5.0f), ImVec2(center.x + 5.0f, center.y + 5.0f), iconColor, 1.35f);
			break;
		case ShellWindowControl::Maximize:
			drawList->AddRect(ImVec2(center.x - 5.0f, center.y - 5.0f), ImVec2(center.x + 5.0f, center.y + 5.0f), iconColor, 0.0f, 0, 1.25f);
			break;
		case ShellWindowControl::Restore:
			drawList->AddRect(ImVec2(center.x - 3.0f, center.y - 6.0f), ImVec2(center.x + 6.0f, center.y + 3.0f), iconColor, 0.0f, 0, 1.1f);
			drawList->AddRect(ImVec2(center.x - 7.0f, center.y - 2.0f), ImVec2(center.x + 2.0f, center.y + 7.0f), iconColor, 0.0f, 0, 1.1f);
			break;
		case ShellWindowControl::Close:
			drawList->AddLine(ImVec2(center.x - 5.0f, center.y - 5.0f), ImVec2(center.x + 5.0f, center.y + 5.0f), iconColor, 1.35f);
			drawList->AddLine(ImVec2(center.x + 5.0f, center.y - 5.0f), ImVec2(center.x - 5.0f, center.y + 5.0f), iconColor, 1.35f);
			break;
		}

		return clicked;
	}

	Ref<Texture2D> GetWhipBrandTexture()
	{
		static Ref<Texture2D> texture = TextureImporter::LoadTexture2D("resources/icons/whip_editor_logo.png");
		return texture;
	}

	void DrawWhipBrandMark(ImDrawList* drawList, const ImVec2& min)
	{
		const ImVec2 max(min.x + 20.0f, min.y + 20.0f);
		if (Ref<Texture2D> texture = GetWhipBrandTexture(); texture && texture->IsLoaded())
		{
			drawList->AddImage(UI::ToImGuiTextureId(texture->GetRendererId()), min, max, ImVec2(0, 1), ImVec2(1, 0));
			return;
		}

		const ImVec2 center((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
		const ImVec2 mark[] =
		{
			ImVec2(center.x, min.y + 2.0f),
			ImVec2(max.x - 3.0f, center.y),
			ImVec2(center.x, max.y - 2.0f),
			ImVec2(min.x + 3.0f, center.y)
		};
		drawList->AddConvexPolyFilled(mark, 4, IM_COL32(245, 248, 252, 245));
		drawList->AddPolyline(mark, 4, IM_COL32(110, 128, 146, 190), ImDrawFlags_Closed, 1.2f);
	}

	std::string LowerCopy(std::string value)
	{
		std::ranges::transform(value, value.begin(),
		                       [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
		return value;
	}

	std::string EditorActionShortcutId(UI::EditorShortcutAction action)
	{
		return std::string("global.") + UI::UISettings::GetActionStorageKey(action);
	}

	bool ShortcutMatchesCommandFilter(const EditorShortcut& shortcut, const char* filter)
	{
		if (!filter || filter[0] == '\0')
			return true;

		std::string needle = LowerCopy(filter);
		std::string haystack = LowerCopy(shortcut.m_DisplayName + " " + shortcut.m_Category + " " + shortcut.m_Id + " " + EditorShortcutManager::GetScopeName(shortcut.m_Scope));
		return haystack.find(needle) != std::string::npos;
	}

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


}

EditorLayer::EditorLayer()
	: Layer("Fbox2D"),
	m_EditorCamera(),
	m_AssetInteractionManager(this),
	m_EntityTemplateManager(this),
	m_HistoryManager(this),
	m_ProjectManager(this),
	m_ScriptManager(this),
	m_SceneManager(this),
	m_PanelManager(this),
	m_ShortcutManager(this),
	m_GizmoType(ImGuizmo::OPERATION::TRANSLATE)
{
}

void EditorLayer::OnAttach()
{
    WHP_PROFILE_FUNCTION();
	WHP_EDITOR_INFO("[Editor] Attaching EditorLayer.");

	m_AnimationEditorPanel.SetRefreshAssetTreeCallback([this]() {if (m_ContentBrowserPanel) { m_ContentBrowserPanel->RefreshAssetTree(); } });
	m_AssetEditorPanel.SetOpenSceneCallback([this](AssetHandle handle) { m_SceneManager.OpenScene(handle); });
	m_AssetEditorPanel.SetSetStartSceneCallback([this](AssetHandle handle) { m_AssetInteractionManager.SetStartScene(handle); });
	m_AssetEditorPanel.SetOpenAnimationCallback([this](AssetHandle handle) { return m_AnimationEditorPanel.OpenAsset(handle, false); });
	m_AssetEditorPanel.SetDrawAnimationEditorCallback([this]() { m_AnimationEditorPanel.OnImGuiRenderEmbedded(); });
	m_AssetEditorPanel.SetRefreshAssetTreeCallback([this]() { if (m_ContentBrowserPanel) { m_ContentBrowserPanel->RefreshAssetTree(); } });
	m_SceneHierarchyPanel.SetSceneChangeCallback([this]() { m_HistoryManager.CaptureSceneHistory(); });
	m_SceneHierarchyPanel.SetSaveEntityTemplateCallback([this](Entity entityIn) { m_EntityTemplateManager.SaveEntityTemplate(entityIn); });
	m_SceneHierarchyPanel.SetApplyEntityTemplateCallback([this](Entity entityIn) { m_EntityTemplateManager.ApplyEntityTemplate(entityIn); });
	m_SceneHierarchyPanel.SetRevertEntityTemplateCallback([this](Entity entityIn) { m_EntityTemplateManager.RevertEntityTemplate(entityIn); });
	m_SceneHierarchyPanel.SetUnpackEntityTemplateCallback([this](Entity entityIn) { m_EntityTemplateManager.UnpackEntityTemplate(entityIn); });
	m_UIProject.SetSceneCallbacks(
		[this](AssetHandle handle) { m_SceneManager.OpenScene(handle); },
		[this]() { m_SceneManager.CloseScene(); },
		[this]() { return m_SceneManager.EditorScenePath(); });
	m_UIProject.SetBeforeChangeCallback([this]() { m_HistoryManager.CaptureSceneHistory(true); });
	m_UIProject.SetEditorSettingsDrawer([this]() { m_UISettings.DrawContent(); });
	m_UISettings.SetShortcutManager(&m_ShortcutManager);
	RegisterEditorShortcuts();
	m_ProjectManager.SetupProjectLoader();
	m_ProjectManager.LoadEditorPreferences();
	m_ProjectManager.GetLoader().SetRecentProjects(m_ProjectManager.GetRecentProjects());

	// framebuffer
    FramebufferSpecification fbSpec{};
    fbSpec.m_Attachments = { FramebufferTextureFormat::Rgba8, FramebufferTextureFormat::RedInteger, FramebufferTextureFormat::Depth };
    fbSpec.m_Width = Application::Get().GetWindow().GetWidth();
    fbSpec.m_Height = Application::Get().GetWindow().GetHeight();
    m_Framebuffer = Framebuffer::Create(fbSpec);

	// scene
	m_SceneManager.ResetToEmptyScene();

	// Project
	auto commandLineArgs = Application::Get().GetSpecification().m_CommandLineArgs;
	if (commandLineArgs.m_Count > 1)
	{
		auto projectFilePath = commandLineArgs[1];
		WHP_EDITOR_INFO(std::string("[Project] Opening project from command line: ") + projectFilePath);
		if (m_ProjectManager.OpenProject(projectFilePath))
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
	m_SceneManager.WriteRecoverySnapshot("Editor shutdown");
	m_ScriptManager.StopSourceWatcher();
	m_ProjectManager.SaveEditorPreferences();
	ConsolePanel::Shutdown();

	if (m_SceneManager.State() == SceneState::Play)
		m_SceneManager.ActiveScene()->OnRuntimeStop();
	else if (m_SceneManager.State() == SceneState::Simulate)
		m_SceneManager.ActiveScene()->OnSimulationStop();

}

void EditorLayer::OnUpdate(Timestep ts)
{
	WHP_PROFILE_FUNCTION();
	m_Ts = ts;
	m_ScriptManager.ProcessSourceChanges(m_SceneManager.State() == SceneState::Edit);
	if (m_SceneManager.IsSceneDirty() && m_SceneManager.State() == SceneState::Edit)
	{
		const auto now = std::chrono::steady_clock::now();
		if (m_SceneManager.LastRecoverySnapshot() == std::chrono::steady_clock::time_point{} || now - m_SceneManager.LastRecoverySnapshot() > std::chrono::seconds(30))
			m_SceneManager.WriteRecoverySnapshot("Autosave");
	}

	{
		WHP_PROFILE_SCOPE("Viewport Size");
		m_SceneManager.ActiveScene()->OnViewportResize(static_cast<uint32_t>(m_ViewportSize.x), static_cast<uint32_t>(m_ViewportSize.y));
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

		switch (m_SceneManager.State())
		{
		case SceneState::Edit:
		{
			if (!m_GizmoUsing)
				m_EditorCamera.OnUpdate(ts);
			DrawEditorGrid();
			m_SceneManager.ActiveScene()->OnUpdateEditor(ts, m_EditorCamera);
			break;
		}
		case SceneState::Play:
		{
			m_SceneManager.ActiveScene()->OnUpdateRuntime(ts);
			m_SceneManager.ProcessRuntimeSceneTransition();
			break;
		}
		case SceneState::Simulate:
		{
			if (!m_GizmoUsing)
				m_EditorCamera.OnUpdate(ts);
			DrawEditorGrid();
			m_SceneManager.ActiveScene()->OnUpdateSimulation(ts, m_EditorCamera);
			m_SceneManager.ProcessRuntimeSceneTransition();
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
			m_HoveredEntity = pixelData == -1 ? Entity() : Entity(static_cast<entt::entity>(pixelData), m_SceneManager.ActiveScene().get());
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
				handledDrop = m_AssetInteractionManager.HandleViewportAssetDrop(assetPayload.m_Handle, assetPayload.m_TextureSpriteIndex);
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
						m_AssetInteractionManager.HandleViewportAssetDrop(handle);
					}
				}
			}
			ImGui::EndDragDropTarget();
		}

		// gizmos
		Entity selectedEntity = m_SceneHierarchyPanel.GetSelectedEntity();
		if (selectedEntity && m_GizmoType != -1 && m_SceneManager.State() != SceneState::Play)
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
				if (!m_HistoryManager.IsGizmoHistoryActive())
				{
					m_HistoryManager.CaptureSceneHistory();
					m_HistoryManager.SetGizmoHistoryActive(true);
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
			m_HistoryManager.SetGizmoHistoryActive(false);
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
	if (Application::Get().GetImGuiLayer()->IsBlockingEvents() && ImGui::GetIO().WantCaptureKeyboard)
	{
		const bool hasActiveWidget = Application::Get().GetImGuiLayer()->GetActiveWidgetID() != 0;
		m_ShortcutManager.HandleImGuiShortcuts(hasActiveWidget);
	}
	DrawCommandPalette();
	if (m_UISettings.ConsumeDirty()
		|| m_ShortcutManager.ConsumeDirty()
		|| m_PanelManager.ConsumeOpenDirty()
		|| m_AnimationEditorPanel.ConsumeLayoutDirty()
		|| m_AssetEditorPanel.ConsumeLayoutDirty()
		|| (m_ContentBrowserPanel && m_ContentBrowserPanel->ConsumePreferencesDirty()))
		m_ProjectManager.SaveEditorPreferences();
	m_PopupHandler.OnImGuiRender();

}
_WHP_PRAGMA_WARNING(pop)

void EditorLayer::OnEvent(Event& event)
{
	if (m_SceneManager.State() == SceneState::Edit && !m_GizmoHovered && !m_GizmoUsing && Application::Get().GetImGuiLayer()->GetActiveWidgetID() == 0)
		m_EditorCamera.OnEvent(event);
    EventDispatcher dispatcher(event);
    dispatcher.Dispatch<KeyPressedEvent>([this](auto&&... args) -> decltype(auto) { return this->OnKeyPressed(std::forward<decltype(args)>(args)...); });
    dispatcher.Dispatch<MouseButtonPressedEvent>([this](auto&&... args) -> decltype(auto) { return this->OnMouseButtonPressed(std::forward<decltype(args)>(args)...); });
	dispatcher.Dispatch<WindowDropEvent>([this](auto&&... args) -> decltype(auto) { return this->OnWindowDrop(std::forward<decltype(args)>(args)...); });
}

bool EditorLayer::OnKeyPressed(KeyPressedEvent& event)
{
	const bool hasActiveWidget = Application::Get().GetImGuiLayer()->GetActiveWidgetID() != 0;
	return m_ShortcutManager.HandleKeyPressed(event, hasActiveWidget);
}

void EditorLayer::DrawEditorShellTitlebar(bool projectLoaded)
{
	constexpr float TitlebarHeight = 30.0f;
	constexpr float ControlWidth = 46.0f;

	ImGui::BeginChild("##EditorShellTitlebar", ImVec2(0.0f, TitlebarHeight), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	const ImVec2 min = ImGui::GetWindowPos();
	const ImVec2 size = ImGui::GetWindowSize();
	const ImVec2 max(min.x + size.x, min.y + size.y);
	ImDrawList* drawList = ImGui::GetWindowDrawList();

	const ImU32 titleTop = IM_COL32(21, 29, 37, 255);
	const ImU32 titleBottom = IM_COL32(8, 12, 16, 255);
	drawList->AddRectFilledMultiColor(min, max, titleTop, titleTop, titleBottom, titleBottom);
	drawList->AddLine(ImVec2(min.x, max.y - 1.0f), ImVec2(max.x, max.y - 1.0f), IM_COL32(46, 58, 70, 210), 1.0f);
	drawList->AddRectFilled(ImVec2(min.x, min.y), ImVec2(max.x, min.y + 2.0f), IM_COL32(180, 196, 214, 210), 0.0f);

	const float controlStartX = max.x - ControlWidth * 3.0f;

	const ImVec2 logoMin(min.x + 12.0f, min.y + 6.0f);
	DrawWhipBrandMark(drawList, logoMin);

	std::string title = "Whip Editor";
	if (projectLoaded && Project::GetActive())
		title += "  /  " + Project::GetActive()->GetConfig().m_Name;
	else
		title += "  /  Hub";

	const float titleX = logoMin.x + 40.0f;
	drawList->AddText(ImVec2(titleX, min.y + 5.0f), IM_COL32(240, 244, 248, 245), title.c_str());
	drawList->AddText(ImVec2(titleX + ImGui::CalcTextSize(title.c_str()).x + 10.0f, min.y + 5.0f), IM_COL32(132, 150, 166, 210), projectLoaded ? "Editor" : "Project Launcher");

	Window& window = Application::Get().GetWindow();
	ImGui::SetCursorScreenPos(ImVec2(controlStartX, min.y));
	if (DrawShellWindowControlButton("##ShellMinimize", ShellWindowControl::Minimize, ImVec2(ControlWidth, TitlebarHeight)))
		window.Minimize();
	ImGui::SameLine(0.0f, 0.0f);
	if (DrawShellWindowControlButton("##ShellMaximize", window.IsMaximized() ? ShellWindowControl::Restore : ShellWindowControl::Maximize, ImVec2(ControlWidth, TitlebarHeight)))
	{
		if (window.IsMaximized())
			window.Restore();
		else
			window.Maximize();
	}
	ImGui::SameLine(0.0f, 0.0f);
	if (DrawShellWindowControlButton("##ShellClose", ShellWindowControl::Close, ImVec2(ControlWidth, TitlebarHeight)))
		Application::Get().Close();

	ImGui::EndChild();
}

void EditorLayer::DrawEditorMenuBar(bool projectLoaded)
{
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.040f, 0.055f, 0.070f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4(0.040f, 0.055f, 0.070f, 1.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::BeginChild("##EditorShellMenuBar", ImVec2(0.0f, 28.0f), false, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	ImGui::PopStyleVar();

	if (ImGui::BeginMenuBar())
	{
		auto drawMenuAction = [this](UI::EditorShortcutAction action, const char* label = nullptr)
			{
				const std::string shortcutId = EditorActionShortcutId(action);
				std::string shortcut = m_ShortcutManager.GetShortcutLabel(shortcutId);
				const bool available = IsEditorActionAvailable(action);
				ImGui::BeginDisabled(!available);
				bool clicked = ImGui::MenuItem(label ? label : UI::UISettings::GetActionDisplayName(action), shortcut.c_str());
				ImGui::EndDisabled();
				m_ShortcutManager.DrawShortcutTooltip(shortcutId, available ? "Run command" : "Command is unavailable in the current editor state.");
				if (clicked)
					ExecuteEditorAction(action);
			};

		if (ImGui::BeginMenu("File"))
		{
			drawMenuAction(UI::EditorShortcutAction::OpenProject);
			drawMenuAction(UI::EditorShortcutAction::SaveProject);
			ImGui::Separator();
			drawMenuAction(UI::EditorShortcutAction::NewScene);
			drawMenuAction(UI::EditorShortcutAction::SaveScene);
			drawMenuAction(UI::EditorShortcutAction::SaveSceneAs, "Save Scene As...");
			drawMenuAction(UI::EditorShortcutAction::CloseScene);
			ImGui::Separator();
			if (ImGui::MenuItem("Restart"))
				Application::Get().SubmitToNextTick([]() { Application::Get().Restart(); });
			if (ImGui::MenuItem("Exit"))
				Application::Get().Close();
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Edit"))
		{
			drawMenuAction(UI::EditorShortcutAction::OpenCommandPalette);
			drawMenuAction(UI::EditorShortcutAction::OpenSettings, "Settings");
			ImGui::Separator();
			drawMenuAction(UI::EditorShortcutAction::Undo);
			drawMenuAction(UI::EditorShortcutAction::Redo);
			ImGui::Separator();
			drawMenuAction(UI::EditorShortcutAction::SelectAll);
			drawMenuAction(UI::EditorShortcutAction::Copy);
			drawMenuAction(UI::EditorShortcutAction::Paste);
			drawMenuAction(UI::EditorShortcutAction::Cut);
			drawMenuAction(UI::EditorShortcutAction::DuplicateEntity);
			drawMenuAction(UI::EditorShortcutAction::DeleteEntity);
			ImGui::Separator();
			ImGui::BeginDisabled(!projectLoaded);
			if (ImGui::MenuItem("Show Animation Editor"))
				m_AnimationEditorPanel.Open();
			ImGui::EndDisabled();
			if (ImGui::MenuItem("Show Test Popup"))
				m_PopupHandler.SetShowState(true);
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Script"))
		{
			drawMenuAction(UI::EditorShortcutAction::ReloadScripts, "Reload Assembly");
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Project"))
		{
			drawMenuAction(UI::EditorShortcutAction::OpenSettings, "Settings");
			ImGui::Separator();
			drawMenuAction(UI::EditorShortcutAction::OpenProject);
			drawMenuAction(UI::EditorShortcutAction::SaveProject);
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Window"))
		{
			RebuildEditorPanelRegistry();
			m_PanelManager.DrawAddPanelMenu(projectLoaded);
			ImGui::Separator();
			ImGui::BeginDisabled(!projectLoaded);
			ImGui::BeginDisabled(!m_AssetEditorPanel.HasOpenEditors());
			if (ImGui::MenuItem("Close Asset Editors"))
				m_AssetEditorPanel.CloseAll();
			ImGui::EndDisabled();
			ImGui::EndDisabled();
			ImGui::EndMenu();
		}

		ImGui::EndMenuBar();
	}

	ImGui::EndChild();
	ImGui::PopStyleColor(2);
}

void EditorLayer::OpenCommandPalette()
{
	m_CommandPaletteOpen = true;
	m_CommandPaletteFocusSearch = true;
	m_CommandPaletteFilter[0] = '\0';
}

void EditorLayer::DrawCommandPalette()
{
	if (!m_CommandPaletteOpen)
		return;

	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f, viewport->WorkPos.y + viewport->WorkSize.y * 0.22f), ImGuiCond_Appearing, ImVec2(0.5f, 0.0f));
	ImGui::SetNextWindowSize(ImVec2(680.0f, 460.0f), ImGuiCond_Appearing);

	bool open = true;
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;
	if (ImGui::Begin("Command Palette", &open, flags))
	{
		if (m_CommandPaletteFocusSearch)
		{
			ImGui::SetKeyboardFocusHere();
			m_CommandPaletteFocusSearch = false;
		}

		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputTextWithHint("##CommandPaletteSearch", "Search commands...", m_CommandPaletteFilter, sizeof(m_CommandPaletteFilter));
		ImGui::SetNextItemWidth(180.0f);
		if (ImGui::BeginCombo("##CommandPaletteScope", m_CommandPaletteScopeFilter < 0 ? "All Scopes" : EditorShortcutManager::GetScopeName(static_cast<EditorShortcutScope>(m_CommandPaletteScopeFilter))))
		{
			if (ImGui::Selectable("All Scopes", m_CommandPaletteScopeFilter < 0))
				m_CommandPaletteScopeFilter = -1;
			for (EditorShortcutScope scope : {
				EditorShortcutScope::Global,
				EditorShortcutScope::Viewport,
				EditorShortcutScope::SceneHierarchy,
				EditorShortcutScope::ContentBrowser,
				EditorShortcutScope::AssetEditor,
				EditorShortcutScope::AnimationEditor,
				EditorShortcutScope::Console,
				EditorShortcutScope::Statistics,
				EditorShortcutScope::ProjectHub })
			{
				const int scopeIndex = static_cast<int>(scope);
				if (ImGui::Selectable(EditorShortcutManager::GetScopeName(scope), m_CommandPaletteScopeFilter == scopeIndex))
					m_CommandPaletteScopeFilter = scopeIndex;
			}
			ImGui::EndCombo();
		}
		ImGui::SameLine();
		ImGui::Checkbox("Runnable only", &m_CommandPaletteAvailableOnly);
		ImGui::SameLine();
		ImGui::TextDisabled("Enter runs first result");
		ImGui::Spacing();
		ImGui::Separator();

		std::string firstAvailableShortcutId;
		bool hasVisibleCommand = false;

		if (ImGui::BeginChild("##CommandPaletteResults", ImVec2(0.0f, 0.0f), false))
		{
			if (ImGui::BeginTable("##CommandPaletteTable", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable))
			{
				ImGui::TableSetupColumn("Command", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthFixed, 130.0f);
				ImGui::TableSetupColumn("Scope", ImGuiTableColumnFlags_WidthFixed, 150.0f);
				ImGui::TableSetupColumn("Shortcut", ImGuiTableColumnFlags_WidthFixed, 150.0f);

				for (const EditorShortcut& shortcut : m_ShortcutManager.GetShortcuts())
				{
					if (shortcut.m_Options.m_HiddenFromCommandPalette || !ShortcutMatchesCommandFilter(shortcut, m_CommandPaletteFilter))
						continue;
					if (m_CommandPaletteScopeFilter >= 0 && static_cast<int>(shortcut.m_Scope) != m_CommandPaletteScopeFilter)
						continue;

					const bool available = m_ShortcutManager.IsShortcutAvailable(shortcut.m_Id, false);
					if (m_CommandPaletteAvailableOnly && !available)
						continue;

					hasVisibleCommand = true;
					if (available && firstAvailableShortcutId.empty())
						firstAvailableShortcutId = shortcut.m_Id;

					ImGui::PushID(shortcut.m_Id.c_str());
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::BeginDisabled(!available);
					if (ImGui::Selectable(shortcut.m_DisplayName.c_str(), false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0.0f, 30.0f)))
					{
						if (m_ShortcutManager.ExecuteShortcut(shortcut.m_Id, false, true, true) && shortcut.m_Id != "global.open_command_palette")
							m_CommandPaletteOpen = false;
					}
					m_ShortcutManager.DrawShortcutTooltip(shortcut.m_Id, available ? "Run command" : "This command is not currently runnable.");
					ImGui::TableNextColumn();
					ImGui::TextDisabled("%s", shortcut.m_Category.c_str());
					ImGui::TableNextColumn();
					ImGui::TextDisabled("%s", EditorShortcutManager::GetScopeName(shortcut.m_Scope));
					ImGui::TableNextColumn();
					const std::string shortcutLabel = EditorShortcutManager::ShortcutLabel(shortcut.m_Binding);
					if (m_ShortcutManager.HasConflict(shortcut.m_Id))
						ImGui::TextColored(ImVec4(0.95f, 0.50f, 0.34f, 1.0f), "%s", m_ShortcutManager.GetConflictDescription(shortcut.m_Id).c_str());
					else if (!available)
						ImGui::TextDisabled("Unavailable");
					else
						ImGui::TextDisabled("%s", shortcutLabel.c_str());
					ImGui::EndDisabled();
					ImGui::PopID();
				}

				ImGui::EndTable();
			}

			if (!hasVisibleCommand)
				ImGui::TextDisabled("No commands found.");
		}
		ImGui::EndChild();

		if (ImGui::IsKeyPressed(ImGuiKey_Escape))
			m_CommandPaletteOpen = false;
		if (!firstAvailableShortcutId.empty() && ImGui::IsKeyPressed(ImGuiKey_Enter))
		{
			if (m_ShortcutManager.ExecuteShortcut(firstAvailableShortcutId, false, true, true) && firstAvailableShortcutId != "global.open_command_palette")
				m_CommandPaletteOpen = false;
		}
	}
	ImGui::End();

	if (!open)
		m_CommandPaletteOpen = false;
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
		m_UIProject.Show(UI::UIProject::UISettings, [this]() { m_ProjectManager.FinishProjectSettings(); });
		return true;
	case UI::EditorShortcutAction::OpenProject:
		m_ProjectManager.OpenProject();
		return true;
	case UI::EditorShortcutAction::NewScene:
		m_SceneManager.NewScene();
		return true;
	case UI::EditorShortcutAction::SaveScene:
		m_SceneManager.SaveScene();
		return true;
	case UI::EditorShortcutAction::SaveSceneAs:
		m_SceneManager.SaveSceneAs();
		return true;
	case UI::EditorShortcutAction::SaveProject:
		m_ProjectManager.SaveProject();
		return true;
	case UI::EditorShortcutAction::CloseScene:
		m_SceneManager.CloseScene();
		return true;
	case UI::EditorShortcutAction::ReloadScripts:
		m_ScriptManager.ReloadAssembly(true, m_SceneManager.State() == SceneState::Edit);
		return true;
	case UI::EditorShortcutAction::DuplicateEntity:
		m_HistoryManager.DuplicateSelection();
		return true;
	case UI::EditorShortcutAction::DeleteEntity:
		m_HistoryManager.DeleteSelection();
		return true;
	case UI::EditorShortcutAction::Undo:
		m_HistoryManager.UndoScene();
		return true;
	case UI::EditorShortcutAction::Redo:
		m_HistoryManager.RedoScene();
		return true;
	case UI::EditorShortcutAction::SelectAll:
		m_HistoryManager.SelectAll();
		return true;
	case UI::EditorShortcutAction::Copy:
		m_HistoryManager.CopySelection();
		return true;
	case UI::EditorShortcutAction::Paste:
		m_HistoryManager.PasteSelection();
		return true;
	case UI::EditorShortcutAction::Cut:
		m_HistoryManager.CutSelection();
		return true;
	case UI::EditorShortcutAction::Play:
		if (m_SceneManager.State() == SceneState::Edit)
			m_SceneManager.OnScenePlay();
		else if (m_SceneManager.State() == SceneState::Play)
			m_SceneManager.OnSceneStop();
		return true;
	case UI::EditorShortcutAction::Simulate:
		if (m_SceneManager.State() == SceneState::Edit)
			m_SceneManager.OnSceneSimulate();
		else if (m_SceneManager.State() == SceneState::Simulate)
			m_SceneManager.OnSceneStop();
		return true;
	case UI::EditorShortcutAction::Stop:
		m_SceneManager.OnSceneStop();
		return true;
	case UI::EditorShortcutAction::Pause:
		m_SceneManager.ActiveScene()->SetPaused(!m_SceneManager.ActiveScene()->IsPaused());
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
	const bool editMode = m_SceneManager.State() == SceneState::Edit;
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
		return projectLoaded && m_SceneManager.State() != SceneState::Simulate;
	case UI::EditorShortcutAction::Simulate:
		return projectLoaded && m_SceneManager.State() != SceneState::Play;
	case UI::EditorShortcutAction::Stop:
		return m_SceneManager.State() == SceneState::Play || m_SceneManager.State() == SceneState::Simulate;
	case UI::EditorShortcutAction::Pause:
		return projectLoaded && m_SceneManager.State() != SceneState::Edit;
	case UI::EditorShortcutAction::GizmoNone:
	case UI::EditorShortcutAction::GizmoTranslate:
	case UI::EditorShortcutAction::GizmoRotate:
	case UI::EditorShortcutAction::GizmoScale:
		return projectLoaded && editMode && !m_GizmoUsing;
	default:
		return false;
	}
}

void EditorLayer::RegisterEditorShortcuts()
{
	m_ShortcutManager.Clear();

	for (size_t i = 0; i < UI::UISettings::ActionCount; ++i)
	{
		const UI::EditorShortcutAction action = static_cast<UI::EditorShortcutAction>(i);
		EditorShortcutOptions options;
		options.m_AllowWhenActiveWidget =
			action == UI::EditorShortcutAction::OpenCommandPalette ||
			action == UI::EditorShortcutAction::Play ||
			action == UI::EditorShortcutAction::Simulate ||
			action == UI::EditorShortcutAction::Stop ||
			action == UI::EditorShortcutAction::Pause;
		options.m_AllowWhenTextInput = action == UI::EditorShortcutAction::OpenCommandPalette;

		m_ShortcutManager.Add(
			EditorShortcutScope::Global,
			EditorActionShortcutId(action),
			UI::UISettings::GetActionDisplayName(action),
			UI::UISettings::GetActionCategory(action),
			m_UISettings.GetShortcutBinding(action),
			[this, action]() { return ExecuteEditorAction(action); },
			[this, action]() { return IsEditorActionAvailable(action); },
			{},
			options);
	}

	m_SceneHierarchyPanel.RegisterShortcuts(m_ShortcutManager);
	m_AnimationEditorPanel.RegisterShortcuts(m_ShortcutManager);
	m_AssetEditorPanel.RegisterShortcuts(m_ShortcutManager);
	auto addConsoleShortcut = [this](const char* id, const char* displayName, const UI::ShortcutBinding& binding, std::function<bool()> callback)
	{
		m_ShortcutManager.Add(
			EditorShortcutScope::Console,
			std::string("console.") + id,
			displayName,
			"Console",
			binding,
			std::move(callback),
			[]() { return ConsolePanel::IsOpen(); },
			[]() { return ConsolePanel::IsShortcutContextActive(); });
	};
	addConsoleShortcut("clear", "Clear Console", { Key::L, true, false, false }, []() { ConsolePanel::Clear(); return true; });
	addConsoleShortcut("copy_visible", "Copy Visible Console Logs", { Key::C, true, true, false }, []() { ConsolePanel::CopyVisible(); return true; });
	addConsoleShortcut("focus_search", "Focus Console Search", { Key::F, true, false, false }, []() { ConsolePanel::FocusSearch(); return true; });
	addConsoleShortcut("clear_filters", "Clear Console Filters", { Key::Backspace, true, false, false }, []() { ConsolePanel::ClearFilters(); return true; });
	addConsoleShortcut("toggle_autoscroll", "Toggle Console Auto-scroll", { Key::A, true, true, false }, []() { ConsolePanel::ToggleAutoScroll(); return true; });
	addConsoleShortcut("show_all", "Console Show All Levels", { Key::D1, true, false, false }, []() { ConsolePanel::ShowAllLevels(); return true; });
	addConsoleShortcut("show_warn_errors", "Console Show Warnings And Errors", { Key::D2, true, false, false }, []() { ConsolePanel::ShowWarningsAndErrors(); return true; });
	addConsoleShortcut("show_errors", "Console Show Errors Only", { Key::D3, true, false, false }, []() { ConsolePanel::ShowErrorsOnly(); return true; });
	if (m_ContentBrowserPanel)
		m_ContentBrowserPanel->RegisterShortcuts(m_ShortcutManager);
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
				m_AssetInteractionManager.HandleViewportAssetDrop(handle);
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
	if (m_SceneManager.State() == SceneState::Play)
	{
		Entity cam = m_SceneManager.ActiveScene()->GetPrimaryCameraEntity();
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
			auto view = m_SceneManager.ActiveScene()->GetAllEntitiesWith<TransformComponent, BoxCollider2DComponent>();
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
			auto view = m_SceneManager.ActiveScene()->GetAllEntitiesWith<TransformComponent, CircleCollider2DComponent>();
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

void EditorLayer::UIToolbar()
{
	bool toolbarEnabled = (bool)m_SceneHierarchyPanel.GetContext();

	ImVec4 tintColor = ImVec4(1, 1, 1, 1);
	if (!toolbarEnabled)
		tintColor.w = 0.5f;

	const SceneState sceneState = m_SceneManager.State();
	bool hasPlayButton = sceneState == SceneState::Edit|| sceneState == SceneState::Play;
	bool hasSimulateButton = sceneState == SceneState::Edit || sceneState == SceneState::Simulate;
	bool hasPauseButton = sceneState != SceneState::Edit;
	bool isPaused = hasPauseButton && m_SceneManager.ActiveScene()->IsPaused();
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

	auto drawIconButton = [&](const char* id, Icon iconType, ImU32 accent, const char* tooltip, UI::EditorShortcutAction action = UI::EditorShortcutAction::Count) -> bool
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
			{
				if (action != UI::EditorShortcutAction::Count)
					m_ShortcutManager.DrawShortcutTooltip(EditorActionShortcutId(action), tooltip);
				else
					ImGui::SetTooltip("%s", tooltip);
			}
			return clicked;
		};

	if(hasPlayButton)
	{
		Icon playIcon = sceneState == SceneState::Play ? Icon::Stop : Icon::Play;
		if (drawIconButton("##ViewportToolbarPlay", playIcon, ColorU32(0.58f, 0.70f, 0.42f, tintColor.w), sceneState == SceneState::Play ? "Stop" : "Play", sceneState == SceneState::Play ? UI::EditorShortcutAction::Stop : UI::EditorShortcutAction::Play))
		{
			if (sceneState == SceneState::Edit || sceneState == SceneState::Simulate)
				m_SceneManager.OnScenePlay();
			else if (sceneState == SceneState::Play)
				m_SceneManager.OnSceneStop();
		}
	}
	if(hasSimulateButton)
	{
		if(hasPlayButton)
			ImGui::SameLine();
		Icon simulateIcon = sceneState == SceneState::Simulate ? Icon::Stop : Icon::Simulate;
		if (drawIconButton("##ViewportToolbarSimulate", simulateIcon, ColorU32(0.66f, 0.55f, 0.42f, tintColor.w), sceneState == SceneState::Simulate ? "Stop simulation" : "Simulate", sceneState == SceneState::Simulate ? UI::EditorShortcutAction::Stop : UI::EditorShortcutAction::Simulate))
		{
			if (sceneState == SceneState::Edit || sceneState == SceneState::Play)
				m_SceneManager.OnSceneSimulate();
			else if (sceneState == SceneState::Simulate)
				m_SceneManager.OnSceneStop();
		}
	}
	if (hasPauseButton)
	{
		ImGui::SameLine();
		if (drawIconButton("##ViewportToolbarPause", Icon::Pause, ColorU32(0.86f, 0.64f, 0.32f, tintColor.w), isPaused ? "Resume" : "Pause", UI::EditorShortcutAction::Pause))
			m_SceneManager.ActiveScene()->SetPaused(!isPaused);

		if (isPaused)
		{
			ImGui::SameLine();
			if (drawIconButton("##ViewportToolbarStepForward", Icon::StepForward, ColorU32(0.86f, 0.64f, 0.32f, tintColor.w), "Step"))
				m_SceneManager.ActiveScene()->Step(m_UISettings.GetStepFrame());
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
