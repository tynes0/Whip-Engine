#pragma once

#include "character.h"

class game_layer : public whip::layer
{
public:
	game_layer();
	virtual ~game_layer() = default;

	virtual void on_attach() override;
	virtual void on_detach() override;
	virtual void on_update(whip::timestep ts) override;
	virtual void on_imgui_render() override;
	virtual void on_event(whip::event& evnt) override;
private:
	whip::orthographic_camera_controller m_camera_controller;
	float tm = 0;

	character m_character;
	character m_monkey;
	whip::ref<whip::texture2D> m_sprite_sheet;
	std::unordered_map<int, whip::ref<whip::sub_texture2D>> m_texture_map;
};