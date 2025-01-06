#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/UUID.h>

#include <box2d/b2_world_callbacks.h>
#include <box2d/b2_contact.h>

#include <Whip/Scene/entity.h>
#include <Whip/Scene/scene.h>
#include <Whip/Scripting/script_engine.h>

#include <coco.h>

_WHIP_START

class contact_listener : public b2ContactListener
{
public:
	void BeginContact(b2Contact* contact) override 
	{
		entt::entity data_a = (entt::entity)contact->GetFixtureA()->GetBody()->GetUserData().pointer;
		entt::entity data_b = (entt::entity)contact->GetFixtureB()->GetBody()->GetUserData().pointer;

		scene* scene_context = script_engine::get_scene_context();
		entity entity_a{ data_a, scene_context };
		entity entity_b{ data_b, scene_context };

		std::string a_tag;
		std::string b_tag;

		if (entity_a.has_component<box_collider2D_component>())
		{
			auto& comp = entity_a.get_component<box_collider2D_component>();
				a_tag = comp.tag;
		}
		else if (entity_a.has_component<circle_collider2D_component>())
		{
			auto& comp = entity_a.get_component<circle_collider2D_component>();
				a_tag = comp.tag;
		}

		if (entity_b.has_component<box_collider2D_component>())
		{
			auto& comp = entity_b.get_component<box_collider2D_component>();
				b_tag = comp.tag;
		}
		else if (entity_b.has_component<circle_collider2D_component>())
		{
			auto& comp = entity_b.get_component<circle_collider2D_component>();
				b_tag = comp.tag;
		}

		auto id_a = entity_a.get_UUID();
		auto id_b = entity_b.get_UUID();

		script_engine::on_collider_enter_entity(entity_a.get_UUID(), b_tag);
		script_engine::on_collider_enter_entity(entity_b.get_UUID(), a_tag);
	}

	void EndContact(b2Contact* contact) override
	{
		entt::entity data_a = (entt::entity)contact->GetFixtureA()->GetBody()->GetUserData().pointer;
		entt::entity data_b = (entt::entity)contact->GetFixtureB()->GetBody()->GetUserData().pointer;

		scene* scene_context = script_engine::get_scene_context();
		entity entity_a{ data_a, scene_context };
		entity entity_b{ data_b, scene_context };

		std::string a_tag;
		std::string b_tag;

		if (entity_a.has_component<box_collider2D_component>())
		{
			auto& comp = entity_a.get_component<box_collider2D_component>();
			if (((b2Fixture*)comp.runtime_fixture) == contact->GetFixtureA())
				a_tag = comp.tag;
		}
		else if (entity_a.has_component<circle_collider2D_component>())
		{
			auto& comp = entity_a.get_component<circle_collider2D_component>();
			if (((b2Fixture*)comp.runtime_fixture) == contact->GetFixtureA())
				a_tag = comp.tag;
		}

		if (entity_b.has_component<box_collider2D_component>())
		{
			auto& comp = entity_b.get_component<box_collider2D_component>();
			if (((b2Fixture*)comp.runtime_fixture) == contact->GetFixtureB())
				b_tag = comp.tag;
		}
		else if (entity_b.has_component<circle_collider2D_component>())
		{
			auto& comp = entity_b.get_component<circle_collider2D_component>();
			if (((b2Fixture*)comp.runtime_fixture) == contact->GetFixtureB())
				b_tag = comp.tag;
		}

		auto id_a = entity_a.get_UUID();
		auto id_b = entity_b.get_UUID();

		script_engine::on_collider_exit_entity(entity_a.get_UUID(), b_tag);
		script_engine::on_collider_exit_entity(entity_b.get_UUID(), a_tag);
	}

	void PreSolve(b2Contact* contact, const b2Manifold* oldManifold) override
	{
		
		b2Fixture* fixture_a = contact->GetFixtureA();
		b2Fixture* fixture_b = contact->GetFixtureB();

		entt::entity data_a = (entt::entity)fixture_a->GetBody()->GetUserData().pointer;
		entt::entity data_b = (entt::entity)fixture_b->GetBody()->GetUserData().pointer;

		scene* scene_context = script_engine::get_scene_context();

		entity entity_a{ data_a, scene_context };
		entity entity_b{ data_b, scene_context };

		contact->ResetFriction();
		contact->ResetRestitution();
		contact->ResetRestitutionThreshold();
	}

	//void PostSolve(b2Contact* contact, const b2ContactImpulse* impulse) override
	//{
	//}

private:
};

_WHIP_END
