#pragma once

#include "Whip/Core/Core.h"
#include "Whip/Scene/Components.h"
#include "Whip/Scene/Entity.h"

#include <box2d/box2d.h>

_WHIP_START

struct PhysicsBodyHandle
{
	b2BodyId m_Id = b2_nullBodyId;
};

struct PhysicsShapeHandle
{
	b2ShapeId m_Id = b2_nullShapeId;
	glm::vec2 m_CachedOffset = { 0.0f, 0.0f };
	glm::vec2 m_CachedSize = { 0.0f, 0.0f };
	glm::vec2 m_CachedScale = { 0.0f, 0.0f };
	float m_CachedRadius = 0.0f;
	float m_CachedDensity = 0.0f;
	float m_CachedFriction = 0.0f;
	float m_CachedRestitution = 0.0f;
	bool m_CachedSensor = false;
};

class Physics2D
{
public:
	static constexpr float COLLIDER_EPSILON = 0.00001f;

	static void SetCollisionFilter(b2ShapeDef& shapeDef, b2BodyType bodyType, bool isSensor);
	static b2BodyType Rigidbody2DTypeToBox2DBody(Rigidbody2DComponent::BodyType type);
	static Rigidbody2DComponent::BodyType Rigidbody2DTypeFromBox2DBody(b2BodyType type);
	static void SetBodyAsSensor(b2BodyId body);
	static b2BodyId CreateBody(Rigidbody2DComponent& rb2d, const TransformComponent& transform, b2WorldId world, uint32_t entityID);
	static void CreateBoxColliderShape(BoxCollider2DComponent& bc2d, const TransformComponent& transform, const Rigidbody2DComponent& rb2d, b2BodyId body);
	static void CreateCircleColliderShape(CircleCollider2DComponent& cc2d, const TransformComponent& transform, const Rigidbody2DComponent& rb2d, b2BodyId body);
	static void UpdateBody(b2BodyId body, const Rigidbody2DComponent& rb2d);
	static void UpdateTransform(TransformComponent& transform, b2BodyId body);
	static void UpdateBoxCollider(BoxCollider2DComponent& bc2d, const TransformComponent& transform, const Rigidbody2DComponent& rb2d, b2BodyId body);
	static void UpdateCircleCollider(CircleCollider2DComponent& cc2d, const TransformComponent& transform, const Rigidbody2DComponent& rb2d, b2BodyId body);

	static b2BodyId GetBodyID(const Rigidbody2DComponent& rb2d);
	static b2BodyId GetBodyID(const void* runtimeBody);
	static PhysicsShapeHandle* GetShapeHandle(void* runtimeShape);
	static const PhysicsShapeHandle* GetShapeHandle(const void* runtimeShape);
	static b2ShapeId GetShapeID(const void* runtimeShape);
	static bool IsShape(const void* runtimeShape, b2ShapeId shape);
	static void DestroyBodyHandle(Rigidbody2DComponent& rb2d);
	static void DestroyShapeHandle(void*& runtimeShape);
};

_WHIP_END
