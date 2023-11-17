#pragma once

#include <Whip.h>

#include "Random.h"

struct particle_props
{
	glm::vec2 position;
	glm::vec2 velocity;
	glm::vec2 velocity_variation;
	glm::vec4 color_begin;
	glm::vec4 color_end;
	float size_begin;
	float size_end;
	float size_variation;
	float life_time = 1.0f;
};

class particle_system
{
public:
	particle_system();

	void emit(const particle_props& pprops);

	void on_update(whip::timestep ts);
	void on_render();
private:
	struct particle
	{
		glm::vec2 position;
		glm::vec2 velocity;
		glm::vec4 color_begin;
		glm::vec4 color_end;
		float rotation = 0.0f;
		float size_begin;
		float size_end;
		float life_time = 1.0f;
		float life_remaining = 0.0f;

		bool active = false;
	};

	std::vector<particle> m_particle_pool;
	uint32_t m_pool_index = 999;
};