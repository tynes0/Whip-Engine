#include "whippch.h"
#include "physics2D.h"

#include "box2d/b2_body.h"
#include "box2d/b2_fixture.h"

_WHIP_START

enum collision_category
{
	STATIC_CATEGORY		= 0x0001,
	KINEMATIC_CATEGORY	= 0x0002,
	DYNAMIC_CATEGORY	= 0x0004,
	SENSOR_CATEGORY		= 0x0008
};

void physics2D::set_collision_filter(b2FixtureDef& fixture_def, b2BodyType body_type, bool is_sensor)
{
	switch (body_type)
	{
	case b2_staticBody:
		fixture_def.filter.categoryBits = STATIC_CATEGORY;
		fixture_def.filter.maskBits = DYNAMIC_CATEGORY | SENSOR_CATEGORY;
		break;

	case b2_kinematicBody:
		fixture_def.filter.categoryBits = KINEMATIC_CATEGORY;
		fixture_def.filter.maskBits = DYNAMIC_CATEGORY | STATIC_CATEGORY | SENSOR_CATEGORY;
		break;

	case b2_dynamicBody:
		fixture_def.filter.categoryBits = DYNAMIC_CATEGORY;
		fixture_def.filter.maskBits = STATIC_CATEGORY | KINEMATIC_CATEGORY | DYNAMIC_CATEGORY | SENSOR_CATEGORY;
		break;
	}
	if (is_sensor)
	{
		fixture_def.filter.categoryBits = SENSOR_CATEGORY;
		fixture_def.filter.maskBits = STATIC_CATEGORY | KINEMATIC_CATEGORY | DYNAMIC_CATEGORY | SENSOR_CATEGORY;
	}
}


b2BodyType physics2D::rigidbody2D_type_to_box2D_body(rigidbody2D_component::body_type type)

{
	switch (type)
	{
	case rigidbody2D_component::body_type::bt_static:    return b2_staticBody;
	case rigidbody2D_component::body_type::bt_dynamic:   return b2_dynamicBody;
	case rigidbody2D_component::body_type::bt_kinematic: return b2_kinematicBody;
	}

	WHP_CORE_ASSERT(false, "Unknown body type!");
	return b2_staticBody;
}

rigidbody2D_component::body_type physics2D::rigidbody2D_type_from_box2D_body(b2BodyType type)
{
	switch (type)
	{
	case b2_staticBody:    return rigidbody2D_component::body_type::bt_static;
	case b2_dynamicBody:   return rigidbody2D_component::body_type::bt_dynamic;
	case b2_kinematicBody: return rigidbody2D_component::body_type::bt_kinematic;
	}

	WHP_CORE_ASSERT(false, "Unknown body type");
	return rigidbody2D_component::body_type::bt_static;
}

void physics2D::set_body_as_sensor(b2Body* body)
{
	body->SetLinearVelocity(b2Vec2_zero);
	if (body->GetType() == b2BodyType::b2_dynamicBody)
		body->SetSleepingAllowed(false);
}

_WHIP_END
