#include "ParticleSystem.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/compatibility.hpp>

particle_system::particle_system()
{
	m_particle_pool.resize(1000);
}

void particle_system::emit(const particle_props& pprops)
{
	particle& x_particle = m_particle_pool[m_pool_index];
	x_particle.active = true;
	x_particle.position = pprops.position;
	x_particle.rotation = random::rfloat() * 2.0f * glm::pi<float>();

	// velocity 

	x_particle.velocity = pprops.velocity;
	x_particle.velocity.x += pprops.velocity_variation.x * (random::rfloat() - 0.5f);
	x_particle.velocity.y += pprops.velocity_variation.y * (random::rfloat() - 0.5f);

	// color

	x_particle.color_begin = pprops.color_begin;
	x_particle.color_end = pprops.color_end;

	// size

	x_particle.size_begin = pprops.size_begin + pprops.size_variation * (random::rfloat() - 0.5f);
	x_particle.size_end = pprops.size_end;

	// life

	x_particle.life_time = pprops.life_time;
	x_particle.life_remaining = pprops.life_time;

	m_pool_index = --m_pool_index % m_particle_pool.size();
}

void particle_system::on_update(whip::timestep ts)
{
	for (auto& x_particle : m_particle_pool)
	{
		if (!x_particle.active)
			continue;
		if (x_particle.life_remaining <= 0.0f)
		{
			x_particle.active;
			continue;
		}
		x_particle.life_remaining -= ts;
		x_particle.position += x_particle.velocity * (float)ts;
		x_particle.rotation += 0.01f * ts;
	}
}

void particle_system::on_render()
{
	for (auto& x_particle : m_particle_pool)
	{
		if (!x_particle.active)
			continue;

		float life = x_particle.life_remaining / x_particle.life_time;
		glm::vec4 x_color = glm::lerp(x_particle.color_end, x_particle.color_begin, life);
		x_color.a = x_color.a * life;

		float size = glm::lerp(x_particle.size_end, x_particle.size_begin, life);
		whip::renderer2D::draw_rotated_quad(x_particle.position, { size, size }, x_particle.rotation, x_color);
	}
}
