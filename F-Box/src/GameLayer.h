#pragma once

#include <Whip.h>

#include "Level.h"
#include <imgui/imgui.h>

class game_layer : public whip::layer
{
public:
	game_layer();

	virtual ~game_layer() = default;
	virtual void on_attach() override;
	virtual void on_detach() override;

	virtual void on_update(whip::timestep ts) override;
	virtual void on_imgui_render() override;
	virtual void on_event(whip::Event& evnt) override;
	
	bool on_mouse_button_pressed(whip::mouse_button_pressed_event& evnt);
	bool on_window_resize(whip::window_resize_event& evnt);
private:
	void create_camera(uint32_t width, uint32_t height);
private:
	whip::scope<whip::orthographic_camera> m_camera;
	level m_level;
	ImFont* m_font;
	float m_time = 0.0f;
	bool m_blink = false;

	enum class game_state
	{
		play = 0, main_menu = 1, game_over = 2
	};

	game_state m_state = game_state::main_menu;
};