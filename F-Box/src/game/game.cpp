#include "game.h"

static const uint32_t s_test_map_w = 20;
static int s_test_map[]
{
	81, 82, 83, -1, 81, 83, -1, -1, -1, 81, 82, 83, -1, -1, -1, -1, -1, -1, -1, -1,
	65, 66, 67, -1, 65, 67, -1, -1, -1, 65, 66, 67, 59, -1, -1, -1, -1, -1, -1, -1,
	49, 50, 51, -1, 49, 51, -1, -1, -1, 49, 50, 51, 28, 13, 13, 13, 13, 13, 13, 13
};


game_layer::game_layer()
	:layer("Game Layer"), m_camera_controller(whip::calculate_aspect_ratio(1280, 720), true)
{
}

void game_layer::on_attach()
{
	whip::logger l_logger = whip::log::create_logger("GAME");
	m_camera_controller.set_camera_position({ 0.0f, 0.0f, 0.0f });
	m_camera_controller.set_zoom_level(5.0f);
	m_camera_controller.set_camera_translation_speed(0.5f);

	m_sprite_sheet = whip::texture2D::create("assets\\game\\textures\\tilemap.png");
	glm::vec2 pixel_diff = { 1.0f, 1.0f };
	glm::vec2 sprite_size = { 18.0f, 18.0f };
	uint32_t vertical_count = static_cast<uint32_t>((m_sprite_sheet->get_height() + pixel_diff.y) / (sprite_size.y + pixel_diff.y));
	uint32_t horizontal_count = static_cast<uint32_t>((m_sprite_sheet->get_width() + pixel_diff.x) / (sprite_size.x + pixel_diff.x));
	int idx = 0;
	for (auto y : whip::irange(vertical_count))
		for (auto x : whip::irange(horizontal_count))
			m_texture_map[idx++] = whip::sub_texture2D::create_from_coords(m_sprite_sheet, { x, y }, sprite_size, pixel_diff);
	m_character.add_still_frame("C:\\Dev\\Whip\\F-Box\\assets\\game\\textures\\character\\frame_1.png");
	m_character.add_changed_movement_frame("C:\\Dev\\Whip\\F-Box\\assets\\game\\textures\\character\\frame_2.png");
	m_character.add_changed_movement_frame("C:\\Dev\\Whip\\F-Box\\assets\\game\\textures\\character\\frame_3.png");
	m_character.add_changed_movement_frame("C:\\Dev\\Whip\\F-Box\\assets\\game\\textures\\character\\frame_4.png");
	m_character.add_attack_frame("C:\\Dev\\Whip\\F-Box\\assets\\game\\textures\\character\\frame_5.png");
	m_character.set_position({ 0.0f, 0.0f, 0.8f });
	m_character.set_default_position({ 0.0f, 0.0f, 0.8f });
	m_character.set_x_face(character::rotation::right);
	m_character.set_speed(m_camera_controller.get_zoom_level() * m_camera_controller.get_camera_translation_speed());

	m_monkey.add_still_frame("C:\\Dev\\Whip\\F-Box\\assets\\game\\textures\\monkey\\right_1.png");
	m_monkey.add_default_movement_frame("C:\\Dev\\Whip\\F-Box\\assets\\game\\textures\\monkey\\left_1.png");
	m_monkey.add_default_movement_frame("C:\\Dev\\Whip\\F-Box\\assets\\game\\textures\\monkey\\left_2.png");
	m_monkey.add_changed_movement_frame("C:\\Dev\\Whip\\F-Box\\assets\\game\\textures\\monkey\\right_1.png");
	m_monkey.add_changed_movement_frame("C:\\Dev\\Whip\\F-Box\\assets\\game\\textures\\monkey\\right_2.png");
	m_monkey.set_position({ 2.0f, 2.0f, 0.7f });
	m_monkey.set_default_position({ 2.0f, 2.0f, 0.7f });
	m_monkey.set_x_face(character::rotation::right);
	m_monkey.set_scale({ 2.0f, 2.0f });
	m_monkey.set_movement_state(true);
	m_monkey.set_speed(m_camera_controller.get_zoom_level() * m_camera_controller.get_camera_translation_speed() * 1.5f);
}

void game_layer::on_detach()
{
}

void game_layer::on_update(whip::timestep ts)
{
	m_camera_controller.on_update(ts);
	if (m_camera_controller.get_zoom_level() * m_camera_controller.get_camera_translation_speed() != m_character.get_speed())
		m_character.set_speed(m_camera_controller.get_zoom_level() * m_camera_controller.get_camera_translation_speed());
	if(m_camera_controller.get_zoom_level() * m_camera_controller.get_camera_translation_speed() * 1.5f != m_monkey.get_speed())
		 m_monkey.set_speed(m_camera_controller.get_zoom_level() * m_camera_controller.get_camera_translation_speed() * 0.5f);
	whip::renderer2D::reset_stats();
	{
		WHP_PROFILE_SCOPE("renderer prep");
		whip::render_command::set_clear_color({ 0.1f, 0.1f, 0.1f, 1.0f });
		whip::render_command::clear();
	}
	
	if (whip::input::is_key_released(WHP_KEY_RIGHT) || whip::input::is_key_released(WHP_KEY_LEFT) || whip::input::is_key_released(WHP_KEY_DOWN) || whip::input::is_key_released(WHP_KEY_UP))
		m_character.set_movement_state(false);
	if (whip::input::is_key_pressed(WHP_KEY_Y))
		m_character.set_attack_state(true);
	if (whip::input::is_key_pressed(WHP_KEY_X))
	{
		m_character.set_movement_state(false);
		m_character.set_attack_state(false);
	}
	float character_speed = ts * 2;
	if (whip::input::is_key_pressed(WHP_KEY_A))
	{
		m_character.set_movement_state(true);
		m_character.move(character::rotation::left);
	}
	if (whip::input::is_key_pressed(WHP_KEY_D))
	{
		m_character.set_movement_state(true);
		m_character.move(character::rotation::right);
	}
	if (whip::input::is_key_pressed(WHP_KEY_W))
	{
		m_character.set_movement_state(true);
		m_character.move(character::rotation::up);
	}
	if (whip::input::is_key_pressed(WHP_KEY_S))
	{
		m_character.set_movement_state(true);
		m_character.move(character::rotation::down);
	}

	whip::renderer2D::begin_scene(m_camera_controller.get_camera());

	whip::renderer2D::draw_quad({ 0, 0, 0 }, { 500.0f, 500.0f }, { 0.2f, 0.5f, 0.2f, 1.0f });
	m_monkey.display(ts);
	m_character.display(ts);

	whip::renderer2D::end_scene();
}

void game_layer::on_imgui_render()
{
}

void game_layer::on_event(whip::event& evnt)
{
	m_camera_controller.on_event(evnt);
}