#include "GameLayer.h"

#include <imgui/imgui.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

game_layer::game_layer() : layer("game layer")
{
	auto& window = whip::application::get().get_window();
	create_camera(window.get_width(), window.get_height());

	random::init();
}

void game_layer::on_attach()
{
	m_level.init();

	ImGuiIO io = GET_IM_IO;
	m_font = io.Fonts->AddFontFromFileTTF("C:\\Dev\\Whip\\F-Box\\assets\\OpenSans-Regular.ttf", 120.0f);
}

void game_layer::on_detach()
{
}

void game_layer::on_update(whip::timestep ts)
{
	m_time += ts;
	if ((int)(m_time * 10.0f) % 8 > 4)
		m_blink = !m_blink;

	if (m_level.is_game_over())
		m_state = game_state::game_over;

	const auto& player_position = m_level.get_player().get_position();
	m_camera->set_position({ player_position.x, player_position.y, 0.0f });

	switch (m_state)
	{
	case game_layer::game_state::play:
		m_level.on_update(ts);
		break;
	}

	whip::render_command::set_clear_color({ 0.0f, 0.0f, 0.0f, 1.0f });
	whip::render_command::clear();

	whip::renderer2D::begin_scene(*m_camera);
	m_level.on_render();
	whip::renderer2D::end_scene();
}

void game_layer::on_imgui_render()
{
	/*ImGui::Begin("Settings");
	m_level.on_imgui_render();
	ImGui::End();*/

	switch (m_state)
	{
		case game_layer::game_state::play:
		{
			uint32_t player_score = m_level.get_player().get_score();
			std::string score_str = std::string("Score: ") + std::to_string(player_score);
			ImGui::GetForegroundDrawList()->AddText(m_font, 48.0f, ImGui::GetWindowPos(), 0xffffffff, score_str.c_str());
			break;
		}
		case game_layer::game_state::main_menu:
		{
			auto pos = ImGui::GetWindowPos();
			auto width = whip::application::get().get_window().get_width();
			auto height = whip::application::get().get_window().get_height();
			pos.x += width * 0.5f - 370.0f;
			pos.x += 50.0f;

			pos.y += 125.0f;

			if (m_blink)
				ImGui::GetForegroundDrawList()->AddText(m_font, 120.0f, pos, 0xffffffff, "Click to Play!");
			break;
		}
		case game_layer::game_state::game_over:
		{
			auto pos = ImGui::GetWindowPos();
			auto width = whip::application::get().get_window().get_width();
			auto height = whip::application::get().get_window().get_height();
			pos.x += width * 0.5f - 370.0f;
			pos.x += 50.0f;

			pos.y += 125.0f;

			if (m_blink)
				ImGui::GetForegroundDrawList()->AddText(m_font, 120.0f, pos, 0xffffffff, "Click to Play!");

			pos.x += 200.0f;
			pos.y += 150.0f;


			uint32_t player_score = m_level.get_player().get_score();
			std::string score_str = std::string("Score: ") + std::to_string(player_score);
			ImGui::GetForegroundDrawList()->AddText(m_font, 48.0f, ImGui::GetWindowPos(), 0xffffffff, score_str.c_str());
			break;
		}
	}
}

void game_layer::on_event(whip::Event& evnt)
{
	whip::event_dispatcher dispatcher(evnt);
	dispatcher.dispatch<whip::window_resize_event>(WHP_BIND_EVENT_FN(game_layer::on_window_resize));
	dispatcher.dispatch<whip::mouse_button_pressed_event>(WHP_BIND_EVENT_FN(game_layer::on_mouse_button_pressed));
}

bool game_layer::on_mouse_button_pressed(whip::mouse_button_pressed_event& evnt)
{
	if (m_state == game_state::game_over)
		m_level.reset();
	m_state = game_state::play;
	return false;
}

bool game_layer::on_window_resize(whip::window_resize_event& evnt)
{
	create_camera(evnt.get_width(), evnt.get_height());
	return false;
}

void game_layer::create_camera(uint32_t width, uint32_t height)
{
	float aspect_ratio = whip::calculate_aspect_ratio((float)width, (float)height);

	float cam_width = 8.0f;
	float bottom = -cam_width;
	float top = cam_width;
	float left = bottom * aspect_ratio;
	float right = top * aspect_ratio;
	m_camera = whip::make_scope<whip::orthographic_camera>(left, right, bottom, top);
}
