#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/UUID.h>

#include <box2d/b2_world_callbacks.h>
#include <box2d/b2_contact.h>

#include <Whip/Scene/Entity.h>
#include <Whip/Scene/Scene.h>
#include <Whip/Scripting/ScriptEngine.h>

#include <coco.h>

_WHIP_START

class ContactFilter : public b2ContactFilter
{
	bool ShouldCollide(b2Fixture* fixtureA, b2Fixture* fixtureB) override;
};

_WHIP_END
