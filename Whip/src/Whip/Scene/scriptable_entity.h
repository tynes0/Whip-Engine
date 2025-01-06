#pragma once

#include <Whip/Core/Core.h>

#include "entity.h"

_WHIP_START

class scriptable_entity
{
public:
	virtual ~scriptable_entity() {}

	template <class T>
	T& get_component()
	{
		return m_entity.get_component<T>();
	}
protected:
	virtual void on_create() {}
	virtual void on_destroy() {}
	virtual void on_update(timestep ts) {}
private:
	friend class scene;

	entity m_entity;
};

_WHIP_END