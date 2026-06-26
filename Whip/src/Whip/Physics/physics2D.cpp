#include "WhipPch.h"
#include "Whip/Physics/Physics2D.h"
#include "Whip/Math/Math.h"

#define NEQ(left, right) (!Math::EqualF((left), (right), Physics2D::COLLIDER_EPSILON))

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

	void CacheBoxShape(PhysicsShapeHandle& handle, const BoxCollider2DComponent& bc2d, const TransformComponent& transform)
	{
		handle.m_CachedOffset = bc2d.m_Offset;
		handle.m_CachedSize = bc2d.m_Size;
		handle.m_CachedScale = { transform.m_Scale.x, transform.m_Scale.y };
		handle.m_CachedDensity = bc2d.m_Density;
		handle.m_CachedFriction = bc2d.m_Friction;
		handle.m_CachedRestitution = bc2d.m_Restitution;
		handle.m_CachedSensor = bc2d.m_Sensor;
	}

	void CacheCircleShape(PhysicsShapeHandle& handle, const CircleCollider2DComponent& cc2d, const TransformComponent& transform)
	{
		handle.m_CachedOffset = cc2d.m_Offset;
		handle.m_CachedScale = { transform.m_Scale.x, transform.m_Scale.y };
		handle.m_CachedRadius = cc2d.m_Radius;
		handle.m_CachedDensity = cc2d.m_Density;
		handle.m_CachedFriction = cc2d.m_Friction;
		handle.m_CachedRestitution = cc2d.m_Restitution;
		handle.m_CachedSensor = cc2d.m_Sensor;
	}

	bool BoxGeometryChanged(const PhysicsShapeHandle& handle, const BoxCollider2DComponent& bc2d, const TransformComponent& transform)
	{
		return NEQ(handle.m_CachedOffset.x, bc2d.m_Offset.x) ||
			NEQ(handle.m_CachedOffset.y, bc2d.m_Offset.y) ||
			NEQ(handle.m_CachedSize.x, bc2d.m_Size.x) ||
			NEQ(handle.m_CachedSize.y, bc2d.m_Size.y) ||
			NEQ(handle.m_CachedScale.x, transform.m_Scale.x) ||
			NEQ(handle.m_CachedScale.y, transform.m_Scale.y);
	}

	bool CircleGeometryChanged(const PhysicsShapeHandle& handle, const CircleCollider2DComponent& cc2d, const TransformComponent& transform)
	{
		return NEQ(handle.m_CachedOffset.x, cc2d.m_Offset.x) ||
			NEQ(handle.m_CachedOffset.y, cc2d.m_Offset.y) ||
			NEQ(handle.m_CachedScale.x, transform.m_Scale.x) ||
			NEQ(handle.m_CachedRadius, cc2d.m_Radius);
	}

	void DestroyRuntimeShape(void*& runtimeShape)
	{
		b2ShapeId shape = Physics2D::GetShapeID(runtimeShape);
		if (b2Shape_IsValid(shape))
			b2DestroyShape(shape, true);
		Physics2D::DestroyShapeHandle(runtimeShape);
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
	const b2BodyType bodyType = Rigidbody2DTypeToBox2DBody(rb2d.m_Type);
	shapeDef.density = bc2d.m_Density;
	shapeDef.material = CreateSurfaceMaterial(bc2d.m_Friction, bc2d.m_Restitution);
	shapeDef.isSensor = bc2d.m_Sensor;
	shapeDef.enableContactEvents = !bc2d.m_Sensor && bodyType != b2_staticBody;
	shapeDef.enableSensorEvents = bc2d.m_Sensor || bodyType != b2_staticBody;
	SetCollisionFilter(shapeDef, bodyType, bc2d.m_Sensor);
	b2Body_SetGravityScale(body, !bc2d.m_Sensor ? rb2d.m_GravityScale : 0.0f);
	if (bc2d.m_Sensor)
		SetBodyAsSensor(body);
	auto* handle = new PhysicsShapeHandle{ b2CreatePolygonShape(body, &shapeDef, &boxShape) };
	CacheBoxShape(*handle, bc2d, transform);
	bc2d.m_RuntimeFixture = handle;
}

void Physics2D::CreateCircleColliderShape(CircleCollider2DComponent& cc2d, const TransformComponent& transform, const Rigidbody2DComponent& rb2d, b2BodyId body)
{
	DestroyShapeHandle(cc2d.m_RuntimeFixture);

	b2Circle circleShape = CreateCircle(cc2d, transform);
	b2ShapeDef shapeDef = b2DefaultShapeDef();
	const b2BodyType bodyType = Rigidbody2DTypeToBox2DBody(rb2d.m_Type);
	shapeDef.density = cc2d.m_Density;
	shapeDef.material = CreateSurfaceMaterial(cc2d.m_Friction, cc2d.m_Restitution);
	shapeDef.isSensor = cc2d.m_Sensor;
	shapeDef.enableContactEvents = !cc2d.m_Sensor && bodyType != b2_staticBody;
	shapeDef.enableSensorEvents = cc2d.m_Sensor || bodyType != b2_staticBody;
	SetCollisionFilter(shapeDef, bodyType, cc2d.m_Sensor);
	b2Body_SetGravityScale(body, !cc2d.m_Sensor ? rb2d.m_GravityScale : 0.0f);
	if (cc2d.m_Sensor)
		SetBodyAsSensor(body);
	auto* handle = new PhysicsShapeHandle{ b2CreateCircleShape(body, &shapeDef, &circleShape) };
	CacheCircleShape(*handle, cc2d, transform);
	cc2d.m_RuntimeFixture = handle;
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

void Physics2D::UpdateBoxCollider(BoxCollider2DComponent& bc2d, const TransformComponent& transform, const Rigidbody2DComponent& rb2d, b2BodyId body)
{
	PhysicsShapeHandle* handle = GetShapeHandle(bc2d.m_RuntimeFixture);
	if (!handle)
		return;

	b2ShapeId shape = handle->m_Id;
	if (!b2Shape_IsValid(shape))
		return;

	if (handle->m_CachedSensor != bc2d.m_Sensor)
	{
		DestroyRuntimeShape(bc2d.m_RuntimeFixture);
		CreateBoxColliderShape(bc2d, transform, rb2d, body);
		return;
	}

	bool massDirty = false;

	if (NEQ(handle->m_CachedDensity, bc2d.m_Density))
	{
		b2Shape_SetDensity(shape, bc2d.m_Density, false);
		handle->m_CachedDensity = bc2d.m_Density;
		massDirty = true;
	}

	if (BoxGeometryChanged(*handle, bc2d, transform))
	{
		const b2Polygon desiredPolygon = CreateBoxPolygon(bc2d, transform);
		b2Shape_SetPolygon(shape, &desiredPolygon);
		handle->m_CachedOffset = bc2d.m_Offset;
		handle->m_CachedSize = bc2d.m_Size;
		handle->m_CachedScale = { transform.m_Scale.x, transform.m_Scale.y };
		massDirty = true;
	}

	if (NEQ(handle->m_CachedFriction, bc2d.m_Friction) || NEQ(handle->m_CachedRestitution, bc2d.m_Restitution))
	{
		b2Shape_SetSurfaceMaterial(shape, CreateSurfaceMaterial(bc2d.m_Friction, bc2d.m_Restitution));
		handle->m_CachedFriction = bc2d.m_Friction;
		handle->m_CachedRestitution = bc2d.m_Restitution;
	}

	if (massDirty && b2Body_IsValid(body))
		b2Body_ApplyMassFromShapes(body);
}

void Physics2D::UpdateCircleCollider(CircleCollider2DComponent& cc2d, const TransformComponent& transform, const Rigidbody2DComponent& rb2d, b2BodyId body)
{
	PhysicsShapeHandle* handle = GetShapeHandle(cc2d.m_RuntimeFixture);
	if (!handle)
		return;

	b2ShapeId shape = handle->m_Id;
	if (!b2Shape_IsValid(shape))
		return;

	if (handle->m_CachedSensor != cc2d.m_Sensor)
	{
		DestroyRuntimeShape(cc2d.m_RuntimeFixture);
		CreateCircleColliderShape(cc2d, transform, rb2d, body);
		return;
	}

	bool massDirty = false;

	if (NEQ(handle->m_CachedDensity, cc2d.m_Density))
	{
		b2Shape_SetDensity(shape, cc2d.m_Density, false);
		handle->m_CachedDensity = cc2d.m_Density;
		massDirty = true;
	}

	if (CircleGeometryChanged(*handle, cc2d, transform))
	{
		const b2Circle desiredCircle = CreateCircle(cc2d, transform);
		b2Shape_SetCircle(shape, &desiredCircle);
		handle->m_CachedOffset = cc2d.m_Offset;
		handle->m_CachedScale = { transform.m_Scale.x, transform.m_Scale.y };
		handle->m_CachedRadius = cc2d.m_Radius;
		massDirty = true;
	}

	if (NEQ(handle->m_CachedFriction, cc2d.m_Friction) || NEQ(handle->m_CachedRestitution, cc2d.m_Restitution))
	{
		b2Shape_SetSurfaceMaterial(shape, CreateSurfaceMaterial(cc2d.m_Friction, cc2d.m_Restitution));
		handle->m_CachedFriction = cc2d.m_Friction;
		handle->m_CachedRestitution = cc2d.m_Restitution;
	}

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

PhysicsShapeHandle* Physics2D::GetShapeHandle(void* runtimeShape)
{
	return static_cast<PhysicsShapeHandle*>(runtimeShape);
}

const PhysicsShapeHandle* Physics2D::GetShapeHandle(const void* runtimeShape)
{
	return static_cast<const PhysicsShapeHandle*>(runtimeShape);
}

b2ShapeId Physics2D::GetShapeID(const void* runtimeShape)
{
	const auto* handle = GetShapeHandle(runtimeShape);
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
