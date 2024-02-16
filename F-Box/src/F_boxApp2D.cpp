#include "F_boxApp2D.h"
#include "Whip/Core/EntryPoint.h"

#include <imgui/imgui.h>
#include <glm/gtc/type_ptr.hpp>
//
//static const uint32_t s_map_width = 23;
//static const char* s_map_tiles =
//"CCCCCCCCCCCCCCCCCCCCCCC"
//"CCCCCCCNCCCCCCCNCCCCCCC"
//"CCCCCCNNNCCCCCNNNCCCCCC"
//"CCCCCNNNNNCCCNNNNNCCCCC"
//"CCCCNNNNNNNCNNNNNNNCCCC"
//"CCCNNNNNNNNNNNNNNNNNCCC"
//"CCCCNNNNNNNNNNNNNNNCCCC"
//"CCCCCNNNNNNNNNNNNNCCCCC"
//"CCCCCCNNNNNNNNNNNCCCCCC"
//"CCCCCCCNNNNNNNNNCCCCCCC"
//"CCCCCCCCNNNNNNNCCCCCCCC"
//"CCCCCCCCCNNNNNCCCCCCCCC"
//"CCCCCCCCCCNNNCCCCCCCCCC"
//"CCCCCCCCCCCNCCCCCCCCCCC"
//"CCCCCCCCCCCCCCCCCCCCCCC";

//static uint32_t s_map_width = 9;
//static const char* s_map_tiles = 
//"YYYYYYYYY"
//"YYXYXYXYY"
//"YXXXXXXYY"
//"YYXXXXXXY"
//"YXXXXXXYY"
//"YYXXXXXXY"
//"YXXXXXXXY"
//"YYXYXYXYY"
//"YYYYYYYYY";

static const uint32_t s_map_w = 17;
static int s_map[]
{
	81, 82, 83, -1, 81, 83, -1, -1, -1, 81, 82, 83, -1, -1, -1, -1, -1,
	65, 66, 67, -1, 65, 67, -1, -1, -1, 65, 66, 67, 59, -1, -1, -1, -1,
	49, 50, 51, -1, 49, 51, -1, -1, -1, 49, 50, 51, 28, 13, 13, 13, 13
};

fbox_app2D::fbox_app2D()
	:layer("Fbox2D"), m_camera_controller(whip::calculate_aspect_ratio(1280, 720), true)
{
}

void fbox_app2D::on_attach()
{
	WHP_PROFILE_FUNCTION();

	m_test_texture = whip::texture2D::create("assets\\textures\\checkerboard.jpg");
	m_test_game_sprite_sheet = whip::texture2D::create("assets\\game\\textures\\tilemap.png");

	glm::vec2 pixel_diff = { 1.0f, 1.0f };
	glm::vec2 sprite_size = { 18.0f, 18.0f };

	uint32_t vertical_count = static_cast<uint32_t>((m_test_game_sprite_sheet->get_height() + pixel_diff.y) / (sprite_size.y + pixel_diff.y));
	uint32_t horizontal_count = static_cast<uint32_t>((m_test_game_sprite_sheet->get_width() + pixel_diff.x) / (sprite_size.x + pixel_diff.x));

	int idx = 0;
	for (auto y : whip::range(vertical_count))
	{
		for (auto x : whip::range(horizontal_count))
		{
			m_test_game_texture_map[idx++] = whip::sub_texture2D::create_from_coords(m_test_game_sprite_sheet, { x, y }, sprite_size, pixel_diff);
		}
	}
	m_camera_controller.set_camera_position({-0.5f, 4.1f, 0.0f});
	m_camera_controller.set_zoom_level(5.0f);

	whip::console_timer sct("standart c++");
	sct.start();
	sct.stop();
	whip::console_timer wct("whip engine");

	wct.start();
	wct.stop();
}

void fbox_app2D::on_detach()
{
	WHP_PROFILE_FUNCTION();
}

void fbox_app2D::on_update(whip::timestep ts)
{
	WHP_PROFILE_FUNCTION();
	//update
	m_camera_controller.on_update(ts);


	whip::renderer2D::reset_stats();
	{
		WHP_PROFILE_SCOPE("renderer prep");
		whip::render_command::set_clear_color({ 0.1f, 0.1f, 0.1f, 1.0f });
		whip::render_command::clear();
	}
	static int a = 50;
	if (a++ == 50)
	{
		a = 0;
		tm = ts.get_milliseconds();
	}

	{
		WHP_PROFILE_SCOPE("renderer draw");

		static float rotation = 0.0f;
		rotation += ts * 45.0f;

		whip::renderer2D::begin_scene(m_camera_controller.get_camera());
		
		whip::renderer2D::draw_quad({ 0, 0, 0 }, { 50.0f, 50.0f }, m_square_color);
		
		for (int32_t x : whip::range(-10, 30))
			whip::renderer2D::draw_quad({ static_cast<float>(x) - s_map_w / 2.0f, (sizeof(s_map) / (sizeof(int)) / s_map_w) / 2.0f - 2, 0.9f }, { 1.1f, 1.1f }, m_test_game_texture_map[93]);

		for (uint32_t y : whip::range(static_cast<uint32_t>(sizeof(s_map) / (sizeof(int)) / s_map_w)))
		{
			for (uint32_t x : whip::range(s_map_w))
			{
				int tile_type = s_map[x + y * s_map_w];
				if (m_test_game_texture_map.find(tile_type) != m_test_game_texture_map.end())
					whip::renderer2D::draw_quad({ static_cast<float>(x) - s_map_w / 2.0f, (sizeof(s_map) / (sizeof(int)) / s_map_w) / 2.0f - static_cast<float>(y), 1.0f }, {1.1f, 1.1f }, m_test_game_texture_map[tile_type], 1);
			}
		}
		whip::renderer2D::end_scene();


		/*for(auto x : whip::range(16))
			for(auto y : whip::range(7))
			whip::renderer2D::draw_quad({ static_cast<float>(x + x), static_cast<float>(y + y), 1.0f}, {1.0f,1.0f}, m_test_game_texture_map[x + y * 16], 1);*/


	}
}

void fbox_app2D::on_imgui_render()
{
	auto stats = whip::renderer2D::get_stats();
	ImGui::Begin("Settings");
	ImGui::ColorEdit4("Square Color", glm::value_ptr(m_square_color));
	ImGui::ColorEdit4("Second item Color", glm::value_ptr(m_second_color));
	ImGui::Text("Draw Calls: %d", stats.draw_calls);
	ImGui::Text("Quads: %d", stats.quad_counts);
	ImGui::Text("Fps: %f", (1000.0f / (tm)));
	ImGui::Text("Vertices: %d", stats.get_total_vertex_count());
	ImGui::Text("Indices: %d", stats.get_total_index_count());
	ImGui::End();
}

void fbox_app2D::on_event(whip::event& evnt)
{
	m_camera_controller.on_event(evnt);
}