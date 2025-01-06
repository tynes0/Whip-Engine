#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Scene/components.h>

enum b2BodyType;
class b2Body;
struct b2FixtureDef;

_WHIP_START

class physics2D
{
public:
	static void set_collision_filter(b2FixtureDef& fixture_def, b2BodyType body_type, bool is_sensor);
	static b2BodyType rigidbody2D_type_to_box2D_body(rigidbody2D_component::body_type type);
	static rigidbody2D_component::body_type rigidbody2D_type_from_box2D_body(b2BodyType type);
	static void set_body_as_sensor(b2Body* body);
};

_WHIP_END
