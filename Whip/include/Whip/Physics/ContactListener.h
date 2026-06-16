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

class ContactListener : public b2ContactListener
{
public:
	void BeginContact(b2Contact* contact) override;

	void EndContact(b2Contact* contact) override;

	void PreSolve(b2Contact* contact, const b2Manifold* oldManifold) override;

	//void PostSolve(b2Contact* contact, const b2ContactImpulse* impulse) override
	//{
	//}

};

_WHIP_END
