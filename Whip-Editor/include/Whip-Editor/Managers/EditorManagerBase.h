#pragma once

#include <Whip.h>

_WHIP_START

class EditorLayer;

class EditorManagerBase // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	EditorManagerBase(EditorLayer* boundedLayer = nullptr)
		: m_BoundedLayer(boundedLayer)
	{
	}

	virtual ~EditorManagerBase() = default;

	void Bind(EditorLayer& layer)
	{
		m_BoundedLayer = &layer;
	}
protected:
	EditorLayer& GetLayer() const
	{
		WHP_CORE_ASSERT(m_BoundedLayer, "This Editor Manager is not bound to an EditorLayer.");
		return *m_BoundedLayer;
	}

	EditorLayer* m_BoundedLayer = nullptr;
};

_WHIP_END
