#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/UUID.h>

#include <box2d/b2_world_callbacks.h>
#include <box2d/b2_contact.h>

#include <Whip/Scene/entity.h>
#include <Whip/Scene/scene.h>
#include <Whip/Scripting/script_engine.h>

#include <coco.h>

_WHIP_START

class contact_filter : public b2ContactFilter
{
	bool ShouldCollide(b2Fixture* fixtureA, b2Fixture* fixtureB) override
	{
		const b2Filter& filterA = fixtureA->GetFilterData();
		const b2Filter& filterB = fixtureB->GetFilterData();

		if (filterA.groupIndex == filterB.groupIndex && filterA.groupIndex != 0)
		{
			return filterA.groupIndex > 0;
		}

		bool collide = (filterA.maskBits & filterB.categoryBits) != 0 && (filterA.categoryBits & filterB.maskBits) != 0;
		return collide;
	}
};

_WHIP_END
