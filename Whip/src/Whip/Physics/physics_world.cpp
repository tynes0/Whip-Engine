#include "whippch.h"

#include "physics_world.h"
#include "physics2D.h"
#include "contact_listener.h"
#include "contact_filter.h"

#include "box2d/b2_world.h"
#include "box2d/b2_body.h"
#include "box2d/b2_fixture.h"
#include "box2d/b2_polygon_shape.h"
#include "box2d/b2_circle_shape.h"


_WHIP_START

static contact_listener s_listener = contact_listener{};
static contact_filter s_filter = contact_filter{};

physics_world::~physics_world()
{
}

void physics_world::set_scene_context(scene* scene_context)
{
	m_scene_context = scene_context;
}

void physics_world::create(float gravity_x, float gravity_y)
{
	if (!private_check(false))
		return;

	m_physics_world = new b2World({ 0.0f, -9.8f });

	m_physics_world->SetContactListener(&s_listener);
	m_physics_world->SetContactFilter(&s_filter);

	auto view = m_scene_context->get_all_entities_with<rigidbody2D_component>();
	for (auto e : view)
	{
		entity ent = { e, m_scene_context };
		auto& transform = ent.get_component<transform_component>();
		auto& rb2d = ent.get_component<rigidbody2D_component>();

		b2BodyDef body_def;
		body_def.type = physics2D::rigidbody2D_type_to_box2D_body(rb2d.type);
		body_def.position.Set(transform.translation.x, transform.translation.y);
		body_def.angle = transform.rotation.z;
		body_def.userData.pointer = (uintptr_t)(entt::entity)ent;
		b2Body* body = m_physics_world->CreateBody(&body_def);
		body->SetFixedRotation(rb2d.fixed_rotation);
		rb2d.runtime_body = body;

		if (ent.has_component<box_collider2D_component>())
		{
			auto& bc2d = ent.get_component<box_collider2D_component>();

			b2PolygonShape box_shape;
			box_shape.SetAsBox(bc2d.size.x * transform.scale.x, bc2d.size.y * transform.scale.y, b2Vec2(bc2d.offset.x, bc2d.offset.y), 0.0f);

			b2FixtureDef fixture_def;
			fixture_def.shape = &box_shape;
			fixture_def.density = bc2d.density;
			fixture_def.friction = bc2d.friction;
			fixture_def.restitution = bc2d.restitution;
			fixture_def.restitutionThreshold = bc2d.restitution_threshold;
			fixture_def.isSensor = bc2d.sensor;
			physics2D::set_collision_filter(fixture_def, body_def.type, bc2d.sensor);
			body->SetGravityScale(!bc2d.sensor ? rb2d.gravity_scale : 0.0f);
			if (bc2d.sensor)
				physics2D::set_body_as_sensor(body);
			bc2d.runtime_fixture = body->CreateFixture(&fixture_def);
		}

		if (ent.has_component<circle_collider2D_component>())
		{
			auto& cc2d = ent.get_component<circle_collider2D_component>();

			b2CircleShape circle_shape;
			circle_shape.m_p.Set(cc2d.offset.x, cc2d.offset.y);
			circle_shape.m_radius = transform.scale.x * cc2d.radius;

			b2FixtureDef fixture_def;
			fixture_def.shape = &circle_shape;
			fixture_def.density = cc2d.density;
			fixture_def.friction = cc2d.friction;
			fixture_def.restitution = cc2d.restitution;
			fixture_def.restitutionThreshold = cc2d.restitution_threshold;
			physics2D::set_collision_filter(fixture_def, body_def.type, cc2d.sensor);
			fixture_def.isSensor = cc2d.sensor;
			body->SetGravityScale(!cc2d.sensor ? rb2d.gravity_scale : 0.0f);
			if (cc2d.sensor)
				physics2D::set_body_as_sensor(body);
			cc2d.runtime_fixture = body->CreateFixture(&fixture_def);
		}
	}
}

void physics_world::update(timestep ts)
{
	if (!private_check(true))
		return;

	static constexpr int32_t velocity_iterations = 6;
	static constexpr int32_t position_iterations = 2;

	m_physics_world->Step(ts, velocity_iterations, position_iterations);

	auto view = m_scene_context->get_all_entities_with<rigidbody2D_component, transform_component>();

	for (auto e : view)
	{
		entity ent = { e, m_scene_context };
		auto& transform = ent.get_component<transform_component>();
		auto& rb2d = ent.get_component<rigidbody2D_component>();
		b2Body* body = (b2Body*)rb2d.runtime_body;

		const auto& position = body->GetPosition();
		transform.translation.x = position.x;
		transform.translation.y = position.y;
		transform.rotation.z = body->GetAngle();

		if (body->IsFixedRotation() != rb2d.fixed_rotation)
			body->SetFixedRotation(rb2d.fixed_rotation);

		{
			if (ent.has_component<box_collider2D_component>())
			{
				auto& bc2d = ent.get_component<box_collider2D_component>();
				b2Fixture* fixture = (b2Fixture*)bc2d.runtime_fixture;
				if (fixture->GetDensity() != bc2d.density)
				{
					fixture->SetDensity(bc2d.density);
					body->ResetMassData();
				}
				b2PolygonShape* shape = dynamic_cast<b2PolygonShape*>(fixture->GetShape());
				if (shape->m_vertices[2] != b2Vec2(bc2d.size.x * transform.scale.x, bc2d.size.y * transform.scale.y) || shape->m_centroid != b2Vec2(bc2d.offset.x, bc2d.offset.y))
					shape->SetAsBox(bc2d.size.x * transform.scale.x, bc2d.size.y * transform.scale.y, b2Vec2(bc2d.offset.x, bc2d.offset.y), 0.0f);
				if (fixture->IsSensor() != bc2d.sensor)
				{
					fixture->SetSensor(bc2d.sensor);
					if (bc2d.sensor)
						physics2D::set_body_as_sensor(body);
				}
				if (fixture->GetFriction() != bc2d.friction)
					fixture->SetFriction(bc2d.friction);
				if (fixture->GetRestitution() != bc2d.restitution)
					fixture->SetRestitution(bc2d.restitution);
				if (fixture->GetRestitutionThreshold() != bc2d.restitution_threshold)
					fixture->SetRestitutionThreshold(bc2d.restitution_threshold);
			}

			if (ent.has_component<circle_collider2D_component>())
			{
				auto& cc2d = ent.get_component<circle_collider2D_component>();
				b2Fixture* fixture = (b2Fixture*)cc2d.runtime_fixture;
				if (fixture->GetDensity() != cc2d.density)
				{
					fixture->SetDensity(cc2d.density);
					body->ResetMassData();
				}
				b2CircleShape* shape = dynamic_cast<b2CircleShape*>(fixture->GetShape());
				if (shape->m_radius != transform.scale.x * cc2d.radius)
					shape->m_radius = transform.scale.x * cc2d.radius;
				if (shape->m_p != b2Vec2{ cc2d.offset.x, cc2d.offset.y })
					shape->m_p.Set(cc2d.offset.x, cc2d.offset.y);
				if (fixture->IsSensor() != cc2d.sensor)
				{
					fixture->SetSensor(cc2d.sensor);
					if (cc2d.sensor)
						physics2D::set_body_as_sensor(body);
				}
				if (fixture->GetFriction() != cc2d.friction)
					fixture->SetFriction(cc2d.friction);
				if (fixture->GetRestitution() != cc2d.restitution)
					fixture->SetRestitution(cc2d.restitution);
				if (fixture->GetRestitutionThreshold() != cc2d.restitution_threshold)
					fixture->SetRestitutionThreshold(cc2d.restitution_threshold);
			}
		}
	}
}

void physics_world::destroy()
{
	if (private_check(true))
		return;

	delete m_physics_world;
	m_physics_world = nullptr;
	auto view = m_scene_context->get_all_entities_with<rigidbody2D_component>();
	for (auto e : view)
	{
		entity ent = { e, m_scene_context };
		if (ent.has_component<box_collider2D_component>())
		{
			auto& bc2d = ent.get_component<box_collider2D_component>();
			bc2d.runtime_fixture = nullptr;
		}
		if (ent.has_component<circle_collider2D_component>())
		{
			auto& cc2d = ent.get_component<circle_collider2D_component>();
			cc2d.runtime_fixture = nullptr;
		}
	}
}

bool physics_world::private_check(bool check_world) const
{
	if (!m_scene_context)
	{
		WHP_CORE_WARN("[Physics World] Scene Context is null!");
		return false;
	}
	if (check_world && !m_physics_world)
	{
		WHP_CORE_WARN("[Physics World] World is not created!");
		return false;
	}
	return true;
}

_WHIP_END
