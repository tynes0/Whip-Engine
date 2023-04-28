#include "Player.h"

#include <imgui/imgui.h>
#include <glm/gtc/matrix_transform.hpp>

player::player()
{
	// smoke

	m_smoke_particle.position = { 0.0f, 0.0f };
	m_smoke_particle.velocity = { -2.0f, 0.0f };
	m_smoke_particle.velocity_variation = { 4.0f, 2.0f };
	m_smoke_particle.size_begin = 0.35f;
	m_smoke_particle.size_end = 0.0f;
	m_smoke_particle.size_variation = 0.15f;
	m_smoke_particle.color_begin = { 0.8f, 0.8f, 0.8f, 1.0f };
	m_smoke_particle.color_end = { 0.6f, 0.6f, 0.6f, 1.0f };
	m_smoke_particle.life_time = 4.0f;

	// flames

	m_engine_particle.position = { 0.0f, 0.0f };
	m_engine_particle.velocity = { -2.0f, 0.0f };
	m_engine_particle.velocity_variation = { 3.0f, 1.0f };
	m_engine_particle.size_begin = 0.5f;
	m_engine_particle.size_end = 0.0f;
	m_engine_particle.size_variation = 0.3f;
	m_engine_particle.color_begin = { 254 / 255.0f, 109 / 255.0f, 41 / 255.0f, 1.0f };
	m_engine_particle.color_end = { 254 / 255.0f, 212 / 255.0f, 123 / 255.0f, 1.0f };
	m_engine_particle.life_time = 1.0f;
}

void player::load_assets()
{
	m_ship_texture = whip::texture2D::create("C:\\Dev\\Whip\\F-Box\\assets\\textures\\Ship.png");
}

void player::on_update(whip::timestep ts)
{
	m_time += ts;
	if (whip::input::is_key_pressed(WHP_KEY_SPACE))
	{
		m_velocity.y += m_engine_power;
		if (m_velocity.y < 0.0f)
		{
			m_velocity.y += m_engine_power * 2.0f;
		}

		// flames

		glm::vec2 emission_point = { 0.0f, -0.6f };
		float rotation = glm::radians(get_rotation());
		glm::vec4 rotated = glm::rotate(glm::mat4(1.0f), rotation, { 0.0f, 0.0f, 1.0f }) * glm::vec4(emission_point, 0.0f, 1.0f);
		m_engine_particle.position = m_position + glm::vec2{ rotated.x, rotated.y };
		m_engine_particle.velocity.y = -m_velocity.y * 0.2f - 0.2f;
		m_particle_system.emit(m_engine_particle);
	}
	else
	{
		m_velocity.y -= m_gravity;
	}

	m_velocity.y = glm::clamp(m_velocity.y, -20.0f, 20.0f);
	m_position += m_velocity * (float)ts;

	// particles

	if (m_time > m_smoke_next_emit_time)
	{
		m_smoke_particle.position = m_position;
		m_particle_system.emit(m_smoke_particle);
		m_smoke_next_emit_time += m_smoke_emit_interval;
	}

	m_particle_system.on_update(ts);
}

void player::on_render()
{
	m_particle_system.on_render();
	whip::renderer2D::draw_rotated_quad({ m_position.x, m_position.y, 0.5f }, { 1.0f, 1.3f }, glm::radians(get_rotation()), m_ship_texture);
}

void player::on_imgui_render()
{
	ImGui::DragFloat("Engine Power", &m_engine_power, 0.1f);
	ImGui::DragFloat("Gravity", &m_gravity);
}

void player::reset()
{
	m_position = { -10.0f, 0.0f };
	m_velocity = { 5.0f, 0.0f };
}
