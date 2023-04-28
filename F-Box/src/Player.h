#pragma once

#include <Whip.h>

#include "Color.h"
#include "Random.h"
#include "ParticleSystem.h"

class player
{
public:
	player();

	void load_assets();

	void on_update(whip::timestep ts);
	void on_render();

	void on_imgui_render();

	void reset();

	float get_rotation() { return m_velocity.y * 4.0f - 90.0f; }
	const glm::vec2& get_position() const { return m_position; }

	uint32_t get_score() const { return (uint32_t)((m_position.x + 10.0f) / 10.0f); }
private:
	glm::vec2 m_position = { -10.0f, 0.0f };
	glm::vec2 m_velocity = { 5.0f, 0.0f };

	float m_engine_power = 0.5f;
	float m_gravity = 0.4f;

	float m_time = 0.0f;
	float m_smoke_emit_interval = 0.4f;
	float m_smoke_next_emit_time = m_smoke_emit_interval;

	particle_props m_smoke_particle;
	particle_props m_engine_particle;
	particle_system m_particle_system;

	whip::ref<whip::texture2D> m_ship_texture;
};