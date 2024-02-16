#pragma once

#include <Whip.h>
#include "ParticleSystem.h"

class fbox_app2D : public whip::layer
{
public:
	fbox_app2D();
	virtual ~fbox_app2D() = default;

	virtual void on_attach() override;
	virtual void on_detach() override;
	virtual void on_update(whip::timestep ts) override;
	virtual void on_imgui_render() override;
	virtual void on_event(whip::event& evnt) override;
private:
	whip::orthographic_camera_controller m_camera_controller;

	//temporary
	whip::ref<whip::shader> m_shader;
	whip::ref<whip::vertex_array> m_vertex_array;
	whip::ref<whip::texture2D> m_test_texture;
	whip::ref<whip::texture2D> m_test_game_sprite_sheet;
	whip::ref<whip::sub_texture2D> m_sprite;
	float tm = 0;

	glm::vec4 m_square_color = { 0.2f, 0.5f, 0.9f, 1.0f };
	glm::vec4 m_second_color = { 0.1f, 0.1f, 0.5f, 1.0f };

	ParticleProps m_particle;
	ParticleSystem m_particle_system;

	std::unordered_map<int, whip::ref<whip::sub_texture2D>> m_test_game_texture_map;
};

