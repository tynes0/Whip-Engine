#include "WhipPch.h"
#include "Whip/Physics/ContactListener.h"
#include "Whip/Physics/Physics2D.h"

_WHIP_START

namespace
{
	Entity GetEntityFromShape(b2ShapeId shape, Scene* scene)
	{
		if (!scene || !b2Shape_IsValid(shape))
			return {};

		b2BodyId body = b2Shape_GetBody(shape);
		if (!b2Body_IsValid(body))
			return {};

		auto* userData = static_cast<BodyUserData*>(b2Body_GetUserData(body));
		if (!userData)
			return {};

		return Entity{ userData->m_EntityID, scene };
	}

	std::string GetColliderTag(Entity entity, b2ShapeId shape)
	{
		if (!entity)
			return {};

		if (entity.HasComponent<BoxCollider2DComponent>())
		{
			auto& comp = entity.GetComponent<BoxCollider2DComponent>();
			if (Physics2D::IsShape(comp.m_RuntimeFixture, shape))
				return comp.m_Tag;
		}
		if (entity.HasComponent<CircleCollider2DComponent>())
		{
			auto& comp = entity.GetComponent<CircleCollider2DComponent>();
			if (Physics2D::IsShape(comp.m_RuntimeFixture, shape))
				return comp.m_Tag;
		}
		if (entity.HasComponent<BoxCollider2DComponent>())
			return entity.GetComponent<BoxCollider2DComponent>().m_Tag;
		if (entity.HasComponent<CircleCollider2DComponent>())
			return entity.GetComponent<CircleCollider2DComponent>().m_Tag;
		return {};
	}

	void InvokeColliderEvent(EntityMethodType methodType, b2ShapeId shapeA, b2ShapeId shapeB)
	{
		Scene* sceneContext = ScriptEngine::GetSceneContext();
		if (!sceneContext)
			return;

		Entity entityA = GetEntityFromShape(shapeA, sceneContext);
		Entity entityB = GetEntityFromShape(shapeB, sceneContext);
		if (!entityA || !entityB)
			return;

		std::string aTag = GetColliderTag(entityA, shapeA);
		std::string bTag = GetColliderTag(entityB, shapeB);

		std::string_view aTagView = aTag;
		std::string_view bTagView = bTag;

		ScriptEngine::InvokeEntityMethod(methodType, entityA, Payload::Ref(bTagView));
		ScriptEngine::InvokeEntityMethod(methodType, entityB, Payload::Ref(aTagView));
	}
}

void ContactListener::ProcessEvents(b2WorldId world) const
{
	WHP_PROFILE_FUNCTION();
	b2ContactEvents contactEvents = b2World_GetContactEvents(world);
	for (int i = 0; i < contactEvents.beginCount; ++i)
	{
		const b2ContactBeginTouchEvent& event = contactEvents.beginEvents[i];
		InvokeColliderEvent(EntityMethodType::OnColliderEnter, event.shapeIdA, event.shapeIdB);
	}
	for (int i = 0; i < contactEvents.endCount; ++i)
	{
		const b2ContactEndTouchEvent& event = contactEvents.endEvents[i];
		if (b2Shape_IsValid(event.shapeIdA) && b2Shape_IsValid(event.shapeIdB))
			InvokeColliderEvent(EntityMethodType::OnColliderExit, event.shapeIdA, event.shapeIdB);
	}

	b2SensorEvents sensorEvents = b2World_GetSensorEvents(world);
	for (int i = 0; i < sensorEvents.beginCount; ++i)
	{
		const b2SensorBeginTouchEvent& event = sensorEvents.beginEvents[i];
		InvokeColliderEvent(EntityMethodType::OnColliderEnter, event.sensorShapeId, event.visitorShapeId);
	}
	for (int i = 0; i < sensorEvents.endCount; ++i)
	{
		const b2SensorEndTouchEvent& event = sensorEvents.endEvents[i];
		if (b2Shape_IsValid(event.sensorShapeId) && b2Shape_IsValid(event.visitorShapeId))
			InvokeColliderEvent(EntityMethodType::OnColliderExit, event.sensorShapeId, event.visitorShapeId);
	}
}

bool ContactListener::PreSolve(b2ShapeId shapeA, b2ShapeId shapeB, b2Manifold* manifold, void* context)
{
	(void)shapeA;
	(void)shapeB;
	(void)manifold;
	(void)context;
	return true;
}


_WHIP_END
