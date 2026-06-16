#include "WhipPch.h"
#include <Whip/Physics/Physics2D.h>

#include "box2d/b2_body.h"
#include "box2d/b2_fixture.h"

_WHIP_START

enum CollisionCategory
{
	StaticCategory		= 0x0001,
	KinematicCategory	= 0x0002,
	DynamicCategory		= 0x0004,
	SensorCategory		= 0x0008
};

void Physics2D::SetCollisionFilter(b2FixtureDef& fixtureDef, b2BodyType bodyType, bool isSensor)
{
	switch (bodyType)
	{
	case b2_staticBody:
		fixtureDef.filter.categoryBits = StaticCategory;
		fixtureDef.filter.maskBits = DynamicCategory | SensorCategory;
		break;

	case b2_kinematicBody:
		fixtureDef.filter.categoryBits = KinematicCategory;
		fixtureDef.filter.maskBits = DynamicCategory | StaticCategory | SensorCategory;
		break;

	case b2_dynamicBody:
		fixtureDef.filter.categoryBits = DynamicCategory;
		fixtureDef.filter.maskBits = StaticCategory | KinematicCategory | DynamicCategory | SensorCategory;
		break;
	}
	if (isSensor)
	{
		fixtureDef.filter.categoryBits = SensorCategory;
		fixtureDef.filter.maskBits = StaticCategory | KinematicCategory | DynamicCategory | SensorCategory;
	}
}


b2BodyType Physics2D::Rigidbody2DTypeToBox2DBody(Rigidbody2DComponent::BodyType type)

{
	switch (type)
	{
	case Rigidbody2DComponent::BodyType::Static:    return b2_staticBody;
	case Rigidbody2DComponent::BodyType::Dynamic:   return b2_dynamicBody;
	case Rigidbody2DComponent::BodyType::Kinematic: return b2_kinematicBody;
	}

	WHP_CORE_ASSERT(false, "Unknown body type!");
	return b2_staticBody;
}

Rigidbody2DComponent::BodyType Physics2D::Rigidbody2DTypeFromBox2DBody(b2BodyType type)
{
	switch (type)
	{
	case b2_staticBody:    return Rigidbody2DComponent::BodyType::Static;
	case b2_dynamicBody:   return Rigidbody2DComponent::BodyType::Dynamic;
	case b2_kinematicBody: return Rigidbody2DComponent::BodyType::Kinematic;
	}

	WHP_CORE_ASSERT(false, "Unknown body type");
	return Rigidbody2DComponent::BodyType::Static;
}

void Physics2D::SetBodyAsSensor(b2Body* body)
{
	body->SetLinearVelocity(b2Vec2_zero);
	if (body->GetType() == b2BodyType::b2_dynamicBody)
		body->SetSleepingAllowed(false);
}

_WHIP_END
