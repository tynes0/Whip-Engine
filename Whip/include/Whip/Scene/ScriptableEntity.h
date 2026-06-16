#pragma once

#include <Whip/Core/Core.h>

#include "Entity.h"

_WHIP_START

class ScriptableEntity
{
public:
	virtual ~ScriptableEntity() {}

	template <class T>
	T& GetComponent()
	{
		return m_Entity.GetComponent<T>();
	}
protected:
	virtual void OnCreate() {}
	virtual void OnDestroy() {}
	virtual void OnUpdate(Timestep ts) {}
private:
	friend class Scene;

	Entity m_Entity;
};

_WHIP_END
