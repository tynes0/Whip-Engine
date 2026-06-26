#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/UUID.h>

#include <box2d/box2d.h>

#include <Whip/Scene/Entity.h>
#include <Whip/Scene/Scene.h>
#include <Whip/Scripting/ScriptEngine.h>

#include <coco.h>

_WHIP_START

class ContactListener
{
public:
	void ProcessEvents(b2WorldId world) const;
	static bool PreSolve(b2ShapeId shapeA, b2ShapeId shapeB, b2Manifold* manifold, void* context);
};

_WHIP_END
