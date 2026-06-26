#include "WhipPch.h"
#include "Whip/Physics/Physics2D.h"
#include "Whip/Math/Math.h"

#define NEQ(left, right) (!Math::EqualF((left), (right), COLLIDER_EPSILON))

_WHIP_START

enum CollisionCategory : uint8_t
{
	CollisionCategory_Static		= 0x0001,
	CollisionCategory_Kinematic		= 0x0002,
	CollisionCategory_Dynamic		= 0x0004,
	CollisionCategory_Sensor		= 0x0008
};

namespace
{
	b2SurfaceMaterial CreateSurfaceMaterial(float friction, float restitution)
	{
		b2SurfaceMaterial material = b2DefaultSurfaceMaterial();
		material.friction = friction;
		material.restitution = restitution;
		return material;
	}

	b2Polygon CreateBoxPolygon(const BoxCollider2DComponent& bc2d, const TransformComponent& transform)
	{
		return b2MakeOffsetBox(
			::abs(bc2d.m_Size.x * transform.m_Scale.x),
			::abs(bc2d.m_Size.y * transform.m_Scale.y),
			{ bc2d.m_Offset.x, bc2d.m_Offset.y },
			b2MakeRot(0.0f));
	}

	b2Circle CreateCircle(const CircleCollider2DComponent& cc2d, const TransformComponent& transform)
	{
		return {
			{ cc2d.m_Offset.x, cc2d.m_Offset.y },
			::abs(transform.m_Scale.x) * cc2d.m_Radius
		};
	}
}

void Physics2D::SetCollisionFilter(b2ShapeDef& shapeDef, b2BodyType bodyType, bool isSensor)
{
	switch (bodyType)
	{
	case b2_staticBody:
		shapeDef.filter.categoryBits = CollisionCategory_Static;
		shapeDef.filter.maskBits = CollisionCategory_Dynamic | CollisionCategory_Sensor;
		break;

	case b2_kinematicBody:
		shapeDef.filter.categoryBits = CollisionCategory_Kinematic;
		shapeDef.filter.maskBits = CollisionCategory_Dynamic | CollisionCategory_Static | CollisionCategory_Sensor;
		break;

	case b2_dynamicBody:
		shapeDef.filter.categoryBits = CollisionCategory_Dynamic;
		shapeDef.filter.maskBits = CollisionCategory_Static | CollisionCategory_Kinematic | CollisionCategory_Dynamic | CollisionCategory_Sensor;
		break;
	}
	if (isSensor)
	{
		shapeDef.filter.categoryBits = CollisionCategory_Sensor;
		shapeDef.filter.maskBits = CollisionCategory_Static | CollisionCategory_Kinematic | CollisionCategory_Dynamic | CollisionCategory_Sensor;
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

void Physics2D::SetBodyAsSensor(b2BodyId body)
{
	if (!b2Body_IsValid(body))
		return;

	b2Body_SetLinearVelocity(body, b2Vec2_zero);
	if (b2Body_GetType(body) == b2_dynamicBody)
		b2Body_EnableSleep(body, false);
}

b2BodyId Physics2D::CreateBody(Rigidbody2DComponent& rb2d, const TransformComponent& transform, b2WorldId world, uint32_t entityID)
{
	DestroyBodyHandle(rb2d);

	b2BodyDef bodyDef = b2DefaultBodyDef();
	bodyDef.type = Rigidbody2DTypeToBox2DBody(rb2d.m_Type);
	bodyDef.position = { transform.m_Translation.x, transform.m_Translation.y };
	bodyDef.rotation = b2MakeRot(transform.m_Rotation.z);
	bodyDef.fixedRotation = rb2d.m_FixedRotation;
	bodyDef.gravityScale = rb2d.m_GravityScale;
	rb2d.m_UserData = std::make_shared<BodyUserData>(static_cast<entt::entity>(entityID));
	bodyDef.userData = rb2d.m_UserData.get();
	b2BodyId body = b2CreateBody(world, &bodyDef);
	rb2d.m_RuntimeBody = new PhysicsBodyHandle{ body };
	return body;
}

void Physics2D::CreateBoxColliderShape(BoxCollider2DComponent& bc2d, const TransformComponent& transform, const Rigidbody2DComponent& rb2d, b2BodyId body)
{
	DestroyShapeHandle(bc2d.m_RuntimeFixture);

	b2Polygon boxShape = CreateBoxPolygon(bc2d, transform);
	b2ShapeDef shapeDef = b2DefaultShapeDef();
	shapeDef.density = bc2d.m_Density;
	shapeDef.material = CreateSurfaceMaterial(bc2d.m_Friction, bc2d.m_Restitution);
	shapeDef.isSensor = bc2d.m_Sensor;
	shapeDef.enableContactEvents = true;
	shapeDef.enableSensorEvents = true;
	SetCollisionFilter(shapeDef, Rigidbody2DTypeToBox2DBody(rb2d.m_Type), bc2d.m_Sensor);
	b2Body_SetGravityScale(body, !bc2d.m_Sensor ? rb2d.m_GravityScale : 0.0f);
	if (bc2d.m_Sensor)
		SetBodyAsSensor(body);
	bc2d.m_RuntimeFixture = new PhysicsShapeHandle{ b2CreatePolygonShape(body, &shapeDef, &boxShape) };
}

void Physics2D::CreateCircleColliderShape(CircleCollider2DComponent& cc2d, const TransformComponent& transform, const Rigidbody2DComponent& rb2d, b2BodyId body)
{
	DestroyShapeHandle(cc2d.m_RuntimeFixture);

	b2Circle circleShape = CreateCircle(cc2d, transform);
	b2ShapeDef shapeDef = b2DefaultShapeDef();
	shapeDef.density = cc2d.m_Density;
	shapeDef.material = CreateSurfaceMaterial(cc2d.m_Friction, cc2d.m_Restitution);
	shapeDef.isSensor = cc2d.m_Sensor;
	shapeDef.enableContactEvents = true;
	shapeDef.enableSensorEvents = true;
	SetCollisionFilter(shapeDef, Rigidbody2DTypeToBox2DBody(rb2d.m_Type), cc2d.m_Sensor);
	b2Body_SetGravityScale(body, !cc2d.m_Sensor ? rb2d.m_GravityScale : 0.0f);
	if (cc2d.m_Sensor)
		SetBodyAsSensor(body);
	cc2d.m_RuntimeFixture = new PhysicsShapeHandle{ b2CreateCircleShape(body, &shapeDef, &circleShape) };
}

void Physics2D::UpdateBody(b2BodyId body, const Rigidbody2DComponent& rb2d)
{
	if (!b2Body_IsValid(body))
		return;

	if (b2Body_IsFixedRotation(body) != rb2d.m_FixedRotation)
		b2Body_SetFixedRotation(body, rb2d.m_FixedRotation);
}

void Physics2D::UpdateTransform(TransformComponent& transform, b2BodyId body)
{
	if (!b2Body_IsValid(body))
		return;

	const b2Vec2 position = b2Body_GetPosition(body);
	transform.m_Translation.x = position.x;
	transform.m_Translation.y = position.y;
	transform.m_Rotation.z = b2Rot_GetAngle(b2Body_GetRotation(body));
}

void Physics2D::UpdateBoxCollider(const BoxCollider2DComponent& bc2d, const TransformComponent& transform, b2BodyId body)
{
	b2ShapeId shape = GetShapeID(bc2d.m_RuntimeFixture);
	if (!b2Shape_IsValid(shape))
		return;

	bool massDirty = false;

	if (NEQ(b2Shape_GetDensity(shape), bc2d.m_Density))
	{
		b2Shape_SetDensity(shape, bc2d.m_Density, false);
		massDirty = true;
	}

	b2Polygon polygon = b2Shape_GetPolygon(shape);
	const b2Polygon desiredPolygon = CreateBoxPolygon(bc2d, transform);
	if (polygon.count != desiredPolygon.count ||
		NEQ(polygon.vertices[2].x, desiredPolygon.vertices[2].x) ||
		NEQ(polygon.vertices[2].y, desiredPolygon.vertices[2].y) ||
		NEQ(polygon.centroid.x, desiredPolygon.centroid.x) ||
		NEQ(polygon.centroid.y, desiredPolygon.centroid.y))
	{
		b2Shape_SetPolygon(shape, &desiredPolygon);
		massDirty = true;
	}

	if (bc2d.m_Sensor)
		SetBodyAsSensor(body);

	b2SurfaceMaterial material = b2Shape_GetSurfaceMaterial(shape);
	if (NEQ(material.friction, bc2d.m_Friction) || NEQ(material.restitution, bc2d.m_Restitution))
		b2Shape_SetSurfaceMaterial(shape, CreateSurfaceMaterial(bc2d.m_Friction, bc2d.m_Restitution));

	if (massDirty && b2Body_IsValid(body))
		b2Body_ApplyMassFromShapes(body);
}

void Physics2D::UpdateCircleCollider(const CircleCollider2DComponent& cc2d, const TransformComponent& transform, b2BodyId body)
{
	b2ShapeId shape = GetShapeID(cc2d.m_RuntimeFixture);
	if (!b2Shape_IsValid(shape))
		return;

	bool massDirty = false;

	if (NEQ(b2Shape_GetDensity(shape), cc2d.m_Density))
	{
		b2Shape_SetDensity(shape, cc2d.m_Density, false);
		massDirty = true;
	}

	b2Circle circle = b2Shape_GetCircle(shape);
	const b2Circle desiredCircle = CreateCircle(cc2d, transform);
	if (NEQ(circle.radius, desiredCircle.radius) ||
		NEQ(circle.center.x, desiredCircle.center.x) ||
		NEQ(circle.center.y, desiredCircle.center.y))
	{
		b2Shape_SetCircle(shape, &desiredCircle);
		massDirty = true;
	}

	if (cc2d.m_Sensor)
		SetBodyAsSensor(body);

	b2SurfaceMaterial material = b2Shape_GetSurfaceMaterial(shape);
	if (NEQ(material.friction, cc2d.m_Friction) || NEQ(material.restitution, cc2d.m_Restitution))
		b2Shape_SetSurfaceMaterial(shape, CreateSurfaceMaterial(cc2d.m_Friction, cc2d.m_Restitution));

	if (massDirty && b2Body_IsValid(body))
		b2Body_ApplyMassFromShapes(body);
}

b2BodyId Physics2D::GetBodyID(const Rigidbody2DComponent& rb2d)
{
	return GetBodyID(rb2d.m_RuntimeBody);
}

b2BodyId Physics2D::GetBodyID(const void* runtimeBody)
{
	const auto* handle = static_cast<const PhysicsBodyHandle*>(runtimeBody);
	return handle ? handle->m_Id : b2_nullBodyId;
}

b2ShapeId Physics2D::GetShapeID(const void* runtimeShape)
{
	const auto* handle = static_cast<const PhysicsShapeHandle*>(runtimeShape);
	return handle ? handle->m_Id : b2_nullShapeId;
}

bool Physics2D::IsShape(const void* runtimeShape, b2ShapeId shape)
{
	b2ShapeId runtimeShapeId = GetShapeID(runtimeShape);
	return B2_IS_NON_NULL(runtimeShapeId) && B2_ID_EQUALS(runtimeShapeId, shape);
}

void Physics2D::DestroyBodyHandle(Rigidbody2DComponent& rb2d)
{
	delete static_cast<PhysicsBodyHandle*>(rb2d.m_RuntimeBody);
	rb2d.m_RuntimeBody = nullptr;
}

void Physics2D::DestroyShapeHandle(void*& runtimeShape)
{
	delete static_cast<PhysicsShapeHandle*>(runtimeShape);
	runtimeShape = nullptr;
}

_WHIP_END
