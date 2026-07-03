#pragma once

#include <Whip.h>

#include "EditorManagerBase.h"

_WHIP_START


class EditorEventManager : public EditorManagerBase // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	explicit EditorEventManager(EditorLayer* boundedLayer = nullptr);
	~EditorEventManager() override;

	bool OnKeyPressed(KeyPressedEvent& event);
	bool OnMouseButtonPressed(MouseButtonPressedEvent& event);
	bool OnWindowDrop(WindowDropEvent& event);
};


_WHIP_END
