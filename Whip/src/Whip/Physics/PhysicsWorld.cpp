#include "WhipPch.h"

#include <Whip/Physics/PhysicsWorld.h>
#include <Whip/Physics/Physics2D.h>
#include <Whip/Physics/ContactListener.h>
#include <Whip/Physics/ContactFilter.h>

#include "box2d/b2_world.h"
#include "box2d/b2_body.h"
#include "box2d/b2_fixture.h"
#include "box2d/b2_polygon_shape.h"
#include "box2d/b2_circle_shape.h"


_WHIP_START

static ContactListener s_Listener = ContactListener{};
static ContactFilter s_Filter = ContactFilter{};

PhysicsWorld::~PhysicsWorld()
{
}

void PhysicsWorld::SetSceneContext(Scene* sceneContext)
{
	m_SceneContext = sceneContext;
}

void PhysicsWorld::Create(float gravityX, float gravityY)
{
	if (!PrivateCheck(false))
		return;

	m_PhysicsWorld = new b2World({ gravityX, -gravityY });

	m_PhysicsWorld->SetContactListener(&s_Listener);
	m_PhysicsWorld->SetContactFilter(&s_Filter);

	auto view = m_SceneContext->GetAllEntitiesWith<Rigidbody2DComponent>();
	for (auto entityHandle : view)
	{
		Entity entity = { entityHandle, m_SceneContext };
		auto& transform = entity.GetComponent<TransformComponent>();
		auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();

		b2BodyDef bodyDef;
		bodyDef.type = Physics2D::Rigidbody2DTypeToBox2DBody(rb2d.m_Type);
		bodyDef.position.Set(transform.m_Translation.x, transform.m_Translation.y);
		bodyDef.angle = transform.m_Rotation.z;
		bodyDef.userData.pointer = (uintptr_t)(entt::entity)entity;
		b2Body* body = m_PhysicsWorld->CreateBody(&bodyDef);
		body->SetFixedRotation(rb2d.m_FixedRotation);
		rb2d.m_RuntimeBody = body;

		if (entity.HasComponent<BoxCollider2DComponent>())
		{
			auto& bc2d = entity.GetComponent<BoxCollider2DComponent>();

			b2PolygonShape boxShape;
			boxShape.SetAsBox(bc2d.m_Size.x * transform.m_Scale.x, bc2d.m_Size.y * transform.m_Scale.y, b2Vec2(bc2d.m_Offset.x, bc2d.m_Offset.y), 0.0f);

			b2FixtureDef fixtureDef;
			fixtureDef.shape = &boxShape;
			fixtureDef.density = bc2d.m_Density;
			fixtureDef.friction = bc2d.m_Friction;
			fixtureDef.restitution = bc2d.m_Restitution;
			fixtureDef.restitutionThreshold = bc2d.m_RestitutionThreshold;
			fixtureDef.isSensor = bc2d.m_Sensor;
			Physics2D::SetCollisionFilter(fixtureDef, bodyDef.type, bc2d.m_Sensor);
			body->SetGravityScale(!bc2d.m_Sensor ? rb2d.m_GravityScale : 0.0f);
			if (bc2d.m_Sensor)
				Physics2D::SetBodyAsSensor(body);
			bc2d.m_RuntimeFixture = body->CreateFixture(&fixtureDef);
		}

		if (entity.HasComponent<CircleCollider2DComponent>())
		{
			auto& cc2d = entity.GetComponent<CircleCollider2DComponent>();

			b2CircleShape circleShape;
			circleShape.m_p.Set(cc2d.m_Offset.x, cc2d.m_Offset.y);
			circleShape.m_radius = transform.m_Scale.x * cc2d.m_Radius;

			b2FixtureDef fixtureDef;
			fixtureDef.shape = &circleShape;
			fixtureDef.density = cc2d.m_Density;
			fixtureDef.friction = cc2d.m_Friction;
			fixtureDef.restitution = cc2d.m_Restitution;
			fixtureDef.restitutionThreshold = cc2d.m_RestitutionThreshold;
			Physics2D::SetCollisionFilter(fixtureDef, bodyDef.type, cc2d.m_Sensor);
			fixtureDef.isSensor = cc2d.m_Sensor;
			body->SetGravityScale(!cc2d.m_Sensor ? rb2d.m_GravityScale : 0.0f);
			if (cc2d.m_Sensor)
				Physics2D::SetBodyAsSensor(body);
			cc2d.m_RuntimeFixture = body->CreateFixture(&fixtureDef);
		}
	}
}

void PhysicsWorld::Update(Timestep ts)
{
	if (!PrivateCheck(true))
		return;

	static constexpr int32_t velocityIterations = 6;
	static constexpr int32_t positionIterations = 2;

	m_PhysicsWorld->Step(ts, velocityIterations, positionIterations);

	auto view = m_SceneContext->GetAllEntitiesWith<Rigidbody2DComponent, TransformComponent>();

	for (auto entityHandle : view)
	{
		Entity entity = { entityHandle, m_SceneContext };
		auto& transform = entity.GetComponent<TransformComponent>();
		auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();
		b2Body* body = (b2Body*)rb2d.m_RuntimeBody;

		const auto& position = body->GetPosition();
		transform.m_Translation.x = position.x;
		transform.m_Translation.y = position.y;
		transform.m_Rotation.z = body->GetAngle();

		if (body->IsFixedRotation() != rb2d.m_FixedRotation)
			body->SetFixedRotation(rb2d.m_FixedRotation);

		{
			if (entity.HasComponent<BoxCollider2DComponent>())
			{
				auto& bc2d = entity.GetComponent<BoxCollider2DComponent>();
				b2Fixture* fixture = (b2Fixture*)bc2d.m_RuntimeFixture;
				if (fixture->GetDensity() != bc2d.m_Density)
				{
					fixture->SetDensity(bc2d.m_Density);
					body->ResetMassData();
				}
				b2PolygonShape* shape = dynamic_cast<b2PolygonShape*>(fixture->GetShape());
				if (shape->m_vertices[2] != b2Vec2(bc2d.m_Size.x * transform.m_Scale.x, bc2d.m_Size.y * transform.m_Scale.y) || shape->m_centroid != b2Vec2(bc2d.m_Offset.x, bc2d.m_Offset.y))
					shape->SetAsBox(bc2d.m_Size.x * transform.m_Scale.x, bc2d.m_Size.y * transform.m_Scale.y, b2Vec2(bc2d.m_Offset.x, bc2d.m_Offset.y), 0.0f);
				if (fixture->IsSensor() != bc2d.m_Sensor)
				{
					fixture->SetSensor(bc2d.m_Sensor);
					if (bc2d.m_Sensor)
						Physics2D::SetBodyAsSensor(body);
				}
				if (fixture->GetFriction() != bc2d.m_Friction)
					fixture->SetFriction(bc2d.m_Friction);
				if (fixture->GetRestitution() != bc2d.m_Restitution)
					fixture->SetRestitution(bc2d.m_Restitution);
				if (fixture->GetRestitutionThreshold() != bc2d.m_RestitutionThreshold)
					fixture->SetRestitutionThreshold(bc2d.m_RestitutionThreshold);
			}

			if (entity.HasComponent<CircleCollider2DComponent>())
			{
				auto& cc2d = entity.GetComponent<CircleCollider2DComponent>();
				b2Fixture* fixture = (b2Fixture*)cc2d.m_RuntimeFixture;
				if (fixture->GetDensity() != cc2d.m_Density)
				{
					fixture->SetDensity(cc2d.m_Density);
					body->ResetMassData();
				}
				b2CircleShape* shape = dynamic_cast<b2CircleShape*>(fixture->GetShape());
				if (shape->m_radius != transform.m_Scale.x * cc2d.m_Radius)
					shape->m_radius = transform.m_Scale.x * cc2d.m_Radius;
				if (shape->m_p != b2Vec2{ cc2d.m_Offset.x, cc2d.m_Offset.y })
					shape->m_p.Set(cc2d.m_Offset.x, cc2d.m_Offset.y);
				if (fixture->IsSensor() != cc2d.m_Sensor)
				{
					fixture->SetSensor(cc2d.m_Sensor);
					if (cc2d.m_Sensor)
						Physics2D::SetBodyAsSensor(body);
				}
				if (fixture->GetFriction() != cc2d.m_Friction)
					fixture->SetFriction(cc2d.m_Friction);
				if (fixture->GetRestitution() != cc2d.m_Restitution)
					fixture->SetRestitution(cc2d.m_Restitution);
				if (fixture->GetRestitutionThreshold() != cc2d.m_RestitutionThreshold)
					fixture->SetRestitutionThreshold(cc2d.m_RestitutionThreshold);
			}
		}
	}
}

void PhysicsWorld::Destroy()
{
	if (!PrivateCheck(true))
		return;

	delete m_PhysicsWorld;
	m_PhysicsWorld = nullptr;
	auto view = m_SceneContext->GetAllEntitiesWith<Rigidbody2DComponent>();
	for (auto entityHandle : view)
	{
		Entity entity = { entityHandle, m_SceneContext };
		if (entity.HasComponent<BoxCollider2DComponent>())
		{
			auto& bc2d = entity.GetComponent<BoxCollider2DComponent>();
			bc2d.m_RuntimeFixture = nullptr;
		}
		if (entity.HasComponent<CircleCollider2DComponent>())
		{
			auto& cc2d = entity.GetComponent<CircleCollider2DComponent>();
			cc2d.m_RuntimeFixture = nullptr;
		}
	}
}

bool PhysicsWorld::PrivateCheck(bool checkWorld) const
{
	if (!m_SceneContext)
	{
		WHP_CORE_WARN("[Physics World] Scene Context is null!");
		return false;
	}
	if (checkWorld && !m_PhysicsWorld)
	{
		WHP_CORE_WARN("[Physics World] World is not created!");
		return false;
	}
	return true;
}

_WHIP_END
