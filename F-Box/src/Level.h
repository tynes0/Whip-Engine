#pragma once

#include "Player.h"

struct pillar
{
	glm::vec3 top_position = { 0.0f, 10.0f, 0.0f };
	glm::vec2 top_scale = { 15.0f, 20.0f };

	glm::vec3 bottom_position = { 10.0f, 10.0f, 0.0f };
	glm::vec2 bottom_scale = { 15.0f, 20.0f };
};

class level
{
public:
	void init();

	void on_update(whip::timestep ts);
	void on_render();

	void on_imgui_render();

	bool is_game_over() const { return m_game_over; }
	void reset();

	player& get_player() { return m_player; }
private:
	void create_pillar(int index, float offset);
	bool collision_test();

	void game_over();
private:
	player m_player;

	bool m_game_over = false;

	float m_pillar_target = 30.0f;
	int m_pillar_index = 0;
	glm::vec3 m_pillar_HSV = { 0.0f, 0.8f, 0.8f };

	std::vector<pillar> m_pillars;

	whip::ref<whip::texture2D> m_triangle_texture;
};