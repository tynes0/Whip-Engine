#include <Whip-Editor/Managers/EditorEventManager.h>
#include <Whip-Editor/EditorLayer.h>

#include "Whip-Editor/Helpers/Utils.h"

#include <imgui.h>

_WHIP_START
	EditorEventManager::EditorEventManager(EditorLayer* boundedLayer)
	: EditorManagerBase(boundedLayer)
{
}

EditorEventManager::~EditorEventManager() = default;

bool EditorEventManager::OnKeyPressed(KeyPressedEvent& event)
{
	EditorLayer& layer = GetLayer();
	const bool hasActiveWidget = Application::Get().GetImGuiLayer()->GetActiveWidgetID() != 0;
	return layer.m_ShortcutManager.HandleKeyPressed(event, hasActiveWidget);
}

bool EditorEventManager::OnMouseButtonPressed(MouseButtonPressedEvent& event)
{
	if (event.GetMouseButton() == Mouse::ButtonLeft)
	{
		EditorLayer& layer = GetLayer();
		if (layer.m_ViewportHovered && !layer.m_GizmoHovered && !layer.m_GizmoUsing && !Input::IsKeyDown(Key::LeftAlt) && Application::Get().GetImGuiLayer()->GetActiveWidgetID() == 0)
		{
			Entity hoveredEntity;
			auto [mx, my] = ImGui::GetMousePos();
			mx -= layer.m_ViewportBounds[0].x;
			my -= layer.m_ViewportBounds[0].y;
			const glm::vec2 viewportSize = layer.m_ViewportBounds[1] - layer.m_ViewportBounds[0];
			my = viewportSize.y - my;
			const int mouseX = static_cast<int>(mx);
			const int mouseY = static_cast<int>(my);

			if (mouseX >= 0 && mouseY >= 0 && mouseX < static_cast<int>(viewportSize.x) && mouseY < static_cast<int>(viewportSize.y))
			{
				layer.m_Framebuffer->Bind();
				const int pixelData = layer.m_Framebuffer->ReadPixel(1, mouseX, mouseY);
				layer.m_Framebuffer->Unbind();
				hoveredEntity = pixelData == -1 ? Entity() : Entity(static_cast<entt::entity>(pixelData), layer.m_SceneManager.ActiveScene().get());
			}

			layer.m_HoveredEntity = hoveredEntity;
			bool append = EditorUtils::IsControlDown();
			layer.m_SceneHierarchyPanel.SetSelectedEntity(hoveredEntity, append);
		}
	}
	return false;
}

bool EditorEventManager::OnWindowDrop(WindowDropEvent& event)
{
	EditorLayer& layer = GetLayer();
	if (!layer.HasProjectLoaded())
		return false;

	if (layer.m_ContentBrowserPanel && layer.m_ContentBrowserPanel->IsHovered())
		return layer.m_ContentBrowserPanel->HandleExternalDrop(event.GetPaths());

	bool handled = false;
	for (const auto& path : event.GetPaths())
	{
		AssetHandle handle = layer.m_AssetInteractionManager.ImportExternalAssetFile(path);
		if (handle != 0)
		{
			handled = true;
			if (layer.m_ViewportHovered)
			{
				if (!layer.m_AssetInteractionManager.HandleViewportAssetDrop(handle))
					WHP_EDITOR_WARN("[Viewport] Drag drop failed.");
			}
		}
	}
	if (layer.m_ContentBrowserPanel)
		layer.m_ContentBrowserPanel->RefreshAssetTree();
	return handled;
}

_WHIP_END
