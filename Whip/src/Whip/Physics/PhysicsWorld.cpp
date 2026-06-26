#include "WhipPch.h"

#include <Whip/Physics/PhysicsWorld.h>
#include <Whip/Physics/Physics2D.h>
#include <Whip/Physics/ContactListener.h>

_WHIP_START

namespace
{
	ContactListener s_Listener = ContactListener{};
}

PhysicsWorld::PhysicsWorld() = default;

PhysicsWorld::PhysicsWorld(Scene* sceneContext)
	: m_SceneContext(sceneContext)
{
}

PhysicsWorld::~PhysicsWorld() = default;

void PhysicsWorld::SetSceneContext(Scene* sceneContext)
{
	m_SceneContext = sceneContext;
}

void PhysicsWorld::Create(float gravityX, float gravityY)
{
	if (!Verify(false))
		return;

	if (b2World_IsValid(m_PhysicsWorld))
		Destroy();

	b2WorldDef worldDef = b2DefaultWorldDef();
	worldDef.gravity = { gravityX, -gravityY };
	m_PhysicsWorld = b2CreateWorld(&worldDef);

	auto view = m_SceneContext->GetAllEntitiesWith<Rigidbody2DComponent>();
	for (auto entityHandle : view)
	{
		Entity entity = { entityHandle, m_SceneContext };

		auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();
		auto& transform = entity.GetComponent<TransformComponent>();

		b2BodyId body = Physics2D::CreateBody(rb2d, transform, m_PhysicsWorld, entity);

		if (entity.HasComponent<BoxCollider2DComponent>())
		{
			auto& bc2d = entity.GetComponent<BoxCollider2DComponent>();
			Physics2D::CreateBoxColliderShape(bc2d, transform, rb2d, body);
		}

		if (entity.HasComponent<CircleCollider2DComponent>())
		{
			auto& cc2d = entity.GetComponent<CircleCollider2DComponent>();
			Physics2D::CreateCircleColliderShape(cc2d, transform, rb2d, body);
		}
	}
}

void PhysicsWorld::Update(Timestep ts) const
{
	if (!Verify(true))
		return;

	static constexpr int32_t subStepCount = 4;

	b2World_Step(m_PhysicsWorld, ts, subStepCount);
	SyncMovedBodyTransforms();
	s_Listener.ProcessEvents(m_PhysicsWorld);

	auto view = m_SceneContext->GetAllEntitiesWith<Rigidbody2DComponent, TransformComponent>();

	for (auto entityHandle : view)
	{
		Entity entity = { entityHandle, m_SceneContext };
		auto& transform = entity.GetComponent<TransformComponent>();
		auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();
		b2BodyId body = Physics2D::GetBodyID(rb2d);
		if (!b2Body_IsValid(body))
			continue;

		Physics2D::UpdateBody(body, rb2d);

		if (entity.HasComponent<BoxCollider2DComponent>())
		{
			auto& bc2d = entity.GetComponent<BoxCollider2DComponent>();
			Physics2D::UpdateBoxCollider(bc2d, transform, rb2d, body);
		}
		if (entity.HasComponent<CircleCollider2DComponent>())
		{
			auto& cc2d = entity.GetComponent<CircleCollider2DComponent>();
			Physics2D::UpdateCircleCollider(cc2d, transform, rb2d, body);
		}
	}
}

void PhysicsWorld::Destroy()
{
	if (!Verify(true))
		return;

	ResetRuntimeHandles();
	b2DestroyWorld(m_PhysicsWorld);
	m_PhysicsWorld = b2_nullWorldId;
}

void PhysicsWorld::ResetRuntimeHandles() const
{
	auto view = m_SceneContext->GetAllEntitiesWith<Rigidbody2DComponent>();
	for (auto entityHandle : view)
	{
		Entity entity = { entityHandle, m_SceneContext };
		if (entity.HasComponent<BoxCollider2DComponent>())
		{
			auto& bc2d = entity.GetComponent<BoxCollider2DComponent>();
			Physics2D::DestroyShapeHandle(bc2d.m_RuntimeFixture);
		}
		if (entity.HasComponent<CircleCollider2DComponent>())
		{
			auto& cc2d = entity.GetComponent<CircleCollider2DComponent>();
			Physics2D::DestroyShapeHandle(cc2d.m_RuntimeFixture);
		}
		auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();
		Physics2D::DestroyBodyHandle(rb2d);
	}
}

void PhysicsWorld::SyncMovedBodyTransforms() const
{
	b2BodyEvents bodyEvents = b2World_GetBodyEvents(m_PhysicsWorld);
	for (int i = 0; i < bodyEvents.moveCount; ++i)
	{
		const b2BodyMoveEvent& event = bodyEvents.moveEvents[i];
		auto* userData = static_cast<BodyUserData*>(event.userData);
		if (!userData)
			continue;

		Entity entity{ userData->m_EntityID, m_SceneContext };
		if (!entity || !entity.HasComponent<TransformComponent>())
			continue;

		auto& transform = entity.GetComponent<TransformComponent>();
		transform.m_Translation.x = event.transform.p.x;
		transform.m_Translation.y = event.transform.p.y;
		transform.m_Rotation.z = b2Rot_GetAngle(event.transform.q);
	}
}

bool PhysicsWorld::Verify(bool checkWorld) const
{
	if (!m_SceneContext)
	{
		WHP_CORE_ERROR("[Physics World] Scene Context is null!");
		return false;
	}
	if (checkWorld && !b2World_IsValid(m_PhysicsWorld))
	{
		WHP_CORE_ERROR("[Physics World] World is not created!");
		return false;
	}
	return true;
}

_WHIP_END
