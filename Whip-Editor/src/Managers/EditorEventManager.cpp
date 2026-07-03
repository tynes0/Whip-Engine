#include <Whip-Editor/Managers/EditorEventManager.h>
#include <Whip-Editor/EditorLayer.h>

#include "Whip-Editor/Helpers/Utils.h"

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
			bool append = EditorUtils::IsControlDown();
			layer.m_SceneHierarchyPanel.SetSelectedEntity(layer.m_HoveredEntity, append);
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
