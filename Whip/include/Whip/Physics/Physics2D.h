#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Scene/Components.h>

enum b2BodyType;
class b2Body;
struct b2FixtureDef;

_WHIP_START

class Physics2D
{
public:
	static void SetCollisionFilter(b2FixtureDef& fixtureDef, b2BodyType bodyType, bool isSensor);
	static b2BodyType Rigidbody2DTypeToBox2DBody(Rigidbody2DComponent::BodyType type);
	static Rigidbody2DComponent::BodyType Rigidbody2DTypeFromBox2DBody(b2BodyType type);
	static void SetBodyAsSensor(b2Body* body);
};

_WHIP_END
