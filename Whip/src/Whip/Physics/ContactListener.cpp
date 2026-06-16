#include "WhipPch.h"
#include "Whip/Physics/ContactListener.h"

_WHIP_START

void ContactListener::BeginContact(b2Contact* contact)
{
	entt::entity dataA = (entt::entity)contact->GetFixtureA()->GetBody()->GetUserData().pointer;
	entt::entity dataB = (entt::entity)contact->GetFixtureB()->GetBody()->GetUserData().pointer;

	Scene* sceneContext = ScriptEngine::GetSceneContext();
	Entity entityA{ dataA, sceneContext };
	Entity entityB{ dataB, sceneContext };

	std::string aTag;
	std::string bTag;

	if (entityA.HasComponent<BoxCollider2DComponent>())
	{
		auto& comp = entityA.GetComponent<BoxCollider2DComponent>();
		aTag = comp.m_Tag;
	}
	else if (entityA.HasComponent<CircleCollider2DComponent>())
	{
		auto& comp = entityA.GetComponent<CircleCollider2DComponent>();
		aTag = comp.m_Tag;
	}

	if (entityB.HasComponent<BoxCollider2DComponent>())
	{
		auto& comp = entityB.GetComponent<BoxCollider2DComponent>();
		bTag = comp.m_Tag;
	}
	else if (entityB.HasComponent<CircleCollider2DComponent>())
	{
		auto& comp = entityB.GetComponent<CircleCollider2DComponent>();
		bTag = comp.m_Tag;
	}

	std::string_view aTagView = aTag;
	std::string_view bTagView = bTag;

	ScriptEngine::InvokeEntityMethod(EntityMethodType::OnColliderEnter, entityA, Payload::Ref(bTagView));
	ScriptEngine::InvokeEntityMethod(EntityMethodType::OnColliderEnter, entityB, Payload::Ref(aTagView));
}

void ContactListener::EndContact(b2Contact* contact)
{
	entt::entity dataA = (entt::entity)contact->GetFixtureA()->GetBody()->GetUserData().pointer;
	entt::entity dataB = (entt::entity)contact->GetFixtureB()->GetBody()->GetUserData().pointer;

	Scene* sceneContext = ScriptEngine::GetSceneContext();
	Entity entityA{ dataA, sceneContext };
	Entity entityB{ dataB, sceneContext };

	std::string aTag;
	std::string bTag;

	if (entityA.HasComponent<BoxCollider2DComponent>())
	{
		auto& comp = entityA.GetComponent<BoxCollider2DComponent>();
		if (static_cast<b2Fixture*>(comp.m_RuntimeFixture) == contact->GetFixtureA())
			aTag = comp.m_Tag;
	}
	else if (entityA.HasComponent<CircleCollider2DComponent>())
	{
		auto& comp = entityA.GetComponent<CircleCollider2DComponent>();
		if (static_cast<b2Fixture*>(comp.m_RuntimeFixture) == contact->GetFixtureA())
			aTag = comp.m_Tag;
	}

	if (entityB.HasComponent<BoxCollider2DComponent>())
	{
		auto& comp = entityB.GetComponent<BoxCollider2DComponent>();
		if (static_cast<b2Fixture*>(comp.m_RuntimeFixture) == contact->GetFixtureB())
			bTag = comp.m_Tag;
	}
	else if (entityB.HasComponent<CircleCollider2DComponent>())
	{
		auto& comp = entityB.GetComponent<CircleCollider2DComponent>();
		if (static_cast<b2Fixture*>(comp.m_RuntimeFixture) == contact->GetFixtureB())
			bTag = comp.m_Tag;
	}

	std::string_view aTagView = aTag;
	std::string_view bTagView = bTag;

	ScriptEngine::InvokeEntityMethod(EntityMethodType::OnColliderExit, entityA, Payload::Ref(bTagView));
	ScriptEngine::InvokeEntityMethod(EntityMethodType::OnColliderExit, entityB, Payload::Ref(aTagView));

}

void ContactListener::PreSolve(b2Contact* contact, const b2Manifold* oldManifold)
{
	contact->ResetFriction();
	contact->ResetRestitution();
	contact->ResetRestitutionThreshold();
}


_WHIP_END
