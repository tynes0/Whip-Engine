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
	m_camera_controller.set_camera_position({ -2.0f, 4.1f, 0.0f });
	m_camera_controller.set_zoom_level(5.0f);
	m_camera_controller.set_camera_translation_speed(1.0f);

	m_sprite_sheet = whip::texture2D::create("assets\\game\\textures\\tilemap.png");
	glm::vec2 pixel_diff = { 1.0f, 1.0f };
	glm::vec2 sprite_size = { 18.0f, 18.0f };
	uint32_t vertical_count = static_cast<uint32_t>((m_sprite_sheet->get_height() + pixel_diff.y) / (sprite_size.y + pixel_diff.y));
	uint32_t horizontal_count = static_cast<uint32_t>((m_sprite_sheet->get_width() + pixel_diff.x) / (sprite_size.x + pixel_diff.x));
	int idx = 0;
	for (auto y : whip::range(vertical_count))
		for (auto x : whip::range(horizontal_count))
			m_texture_map[idx++] = whip::sub_texture2D::create_from_coords(m_sprite_sheet, { x, y }, sprite_size, pixel_diff);
	m_character.add_still_frame("C:\\Dev\\Whip\\F-Box\\assets\\game\\textures\\character\\frame_1.png");
	m_character.add_changed_movement_frame("C:\\Dev\\Whip\\F-Box\\assets\\game\\textures\\character\\frame_2.png");
	m_character.add_changed_movement_frame("C:\\Dev\\Whip\\F-Box\\assets\\game\\textures\\character\\frame_3.png");
	m_character.add_changed_movement_frame("C:\\Dev\\Whip\\F-Box\\assets\\game\\textures\\character\\frame_4.png");
	m_character.add_attack_frame("C:\\Dev\\Whip\\F-Box\\assets\\game\\textures\\character\\frame_5.png");
	m_character.set_position({ 0 - s_test_map_w / 2.0f + 1.0f, (sizeof(s_test_map) / (sizeof(int)) / s_test_map_w) / 2.0f + 1, 0.8f });
	m_character.set_default_position({ 0 - s_test_map_w / 2.0f + 1.0f, (sizeof(s_test_map) / (sizeof(int)) / s_test_map_w) / 2.0f + 1, 0.8f });
	m_character.set_x_face(character::rotation::right);

	auto tr = whip::filesystem::filepath_parser::fetch_extension("C:\\Dev\\Whip\\F-Box\\assets\\game\\textures\\character\\frame_5.");
	WHP_CORE_DEBUG("{0}", tr);
}

void game_layer::on_detach()
{
}

void game_layer::on_update(whip::timestep ts)
{
	m_camera_controller.on_update(ts);
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

	if (whip::input::is_key_pressed(WHP_KEY_LEFT))
	{
		m_character.set_movement_state(true);
		m_character.move(character::rotation::left, ts);
	}
	if (whip::input::is_key_pressed(WHP_KEY_RIGHT))
	{
		m_character.set_movement_state(true);
		m_character.move(character::rotation::right, ts);
	}
	if (whip::input::is_key_pressed(WHP_KEY_UP))
	{
		m_character.set_movement_state(true);
		m_character.move(character::rotation::up, ts);
	}
	if (whip::input::is_key_pressed(WHP_KEY_DOWN))
	{
		m_character.set_movement_state(true);
		m_character.move(character::rotation::down, ts);
	}

	whip::renderer2D::begin_scene(m_camera_controller.get_camera());

	whip::renderer2D::draw_quad({ 0, 0, 0 }, { 500.0f, 500.0f }, { 0.5f, 0.5f, 0.5f, 1.0f });

	m_character.display(ts);
	for (int32_t x : whip::range(-10, 30))
		whip::renderer2D::draw_quad({ static_cast<float>(x) - s_test_map_w / 2.0f, (sizeof(s_test_map) / (sizeof(int)) / s_test_map_w) / 2.0f - 2, 0.9f }, { 1.1f, 1.1f }, m_texture_map[93]);

	for (uint32_t y : whip::range(static_cast<uint32_t>(sizeof(s_test_map) / (sizeof(int)) / s_test_map_w)))
	{
		for (uint32_t x : whip::range(s_test_map_w))
		{
			int tile_type = s_test_map[x + y * s_test_map_w];
			if (m_texture_map.find(tile_type) != m_texture_map.end())
				whip::renderer2D::draw_quad({ static_cast<float>(x) - s_test_map_w / 2.0f, (sizeof(s_test_map) / (sizeof(int)) / s_test_map_w) / 2.0f - static_cast<float>(y), 1.0f }, { 1.1f, 1.1f }, m_texture_map[tile_type], 1);
		}
	}

	whip::renderer2D::end_scene();
}

void game_layer::on_imgui_render()
{
}

void game_layer::on_event(whip::event& evnt)
{
	m_camera_controller.on_event(evnt);
}