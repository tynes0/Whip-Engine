#include "F_boxApp2D.h"
#include "Whip/Core/EntryPoint.h"

#include <imgui/imgui.h>
#include <glm/gtc/type_ptr.hpp>

static const uint32_t s_map_width = 23;
static const char* s_map_tiles =
"CCCCCCCCCCCCCCCCCCCCCCC"
"CCCCCCCCCCCCCCCCCCCCCCC"
"CCCCNNNNNCCCCCCCNNNCCCC"
"CCCCNNNNNNCCCCCCNNNCCCC"
"CCCCNNNNNNNCCCCCNNNCCCC"
"CCCCNNNCNNNNCCCCNNNCCCC"
"CCCCNNNCCNNNNCCCNNNCCCC"
"CCCCNNNCCCNNNNCCNNNCCCC"
"CCCCNNNCCCCNNNNCNNNCCCC"
"CCCCNNNCCCCCNNNNNNNCCCC"
"CCCCNNNCCCCCCNNNNNNCCCC"
"CCCCNNNCCCCCCCNNNNNCCCC"
"CCCCCCCCCCCCCCCCCCCCCCC"
"CCCCCCCCCCCCCCCCCCCCCCC";

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

fbox_app2D::fbox_app2D()
	:layer("Fbox2D"), m_camera_controller(whip::calculate_aspect_ratio(1280, 720), true), m_map_height(0), m_map_width(0)
{
}

void fbox_app2D::on_attach()
{
	WHP_PROFILE_FUNCTION();

	m_test_texture = whip::texture2D::create("assets\\textures\\checkerboard.jpg");
	m_sprite_sheet = whip::texture2D::create("assets\\game\\textures\\sprite.png");
	m_sprite = whip::sub_texture2D::create_from_coords(m_sprite_sheet, { 1.0f,1.0f }, { 128.0f,128.0f });

	m_texture_map['N'] = whip::sub_texture2D::create_from_coords(m_sprite_sheet, {  5.0f,5.0f }, {128.0f,128.0f});
	m_texture_map['C'] = whip::sub_texture2D::create_from_coords(m_sprite_sheet, { 14.0f,5.0f }, {128.0f,128.0f});
	m_texture_map['Y'] = whip::sub_texture2D::create_from_coords(m_sprite_sheet, { 6.0f,11.0f }, { 128.0f,128.0f });
	m_texture_map['X'] = whip::sub_texture2D::create_from_coords(m_sprite_sheet, { 1.0f,11.0f }, { 128.0f,128.0f });


	m_map_width = s_map_width;
	m_map_height = static_cast<uint32_t>(strlen(s_map_tiles) / s_map_width);

	m_camera_controller.set_zoom_level(5.0f);

	whip::console_timer sct("standart c++");

	sct.start();

	sct.stop();

	whip::console_timer ct("whip engine");

	ct.start();

	ct.stop();
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

	
	{
		WHP_PROFILE_SCOPE("renderer draw");

		static float rotation = 0.0f;
		rotation += ts * 45.0f;

		whip::renderer2D::begin_scene(m_camera_controller.get_camera());
		/*
		whip::renderer2D::draw_quad({ -1.0f, 0.0f }, { 1.0f, 1.0f }, m_square_color);
		whip::renderer2D::draw_rotated_quad({ 1.0f, 0.0f }, { 1.0f, 1.0f }, (rotation / 10), m_second_color);

		whip::renderer2D::draw_quad({ 0.0f, 0.0f, -0.1 }, { 100.0f, 100.0f }, m_test_texture, 10.0f);
		*/
		for (uint32_t y : whip::range(m_map_height))
		{
			for (uint32_t x : whip::range(m_map_width))
			{
				char tile_type = s_map_tiles[x + y * m_map_width];
				whip::ref<whip::sub_texture2D> texture;
				if (m_texture_map.find(tile_type) != m_texture_map.end())
					texture = m_texture_map[tile_type];
				else
					texture = m_sprite;
				whip::renderer2D::draw_quad({ static_cast<float>(x) - m_map_width / 2.0f, m_map_height / 2.0f - static_cast<float>(y), 0.5f }, { 1.0f,1.0f }, texture);
			}
		}
		whip::renderer2D::end_scene();
	}

	/*whip::renderer2D::begin_scene(m_camera_controller.get_camera());

	for (uint32_t y = 0; y < m_map_height; ++y)
	{
		for (uint32_t x = 0; x < m_map_width; ++x)
		{
			char tile_type = s_map_tiles[x + y * m_map_width];
			whip::ref<whip::sub_texture2D> texture;
			if (m_texture_map.find(tile_type) != m_texture_map.end())
				texture = m_texture_map[tile_type];
			else
				texture = m_sprite;
			whip::renderer2D::draw_quad({ x - m_map_width / 2.0f, m_map_height - y - m_map_height / 2.0f, 0.1f }, { 1.0f, 1.0f }, texture);
		}
	}

	whip::renderer2D::end_scene();*/
}

void fbox_app2D::on_imgui_render()
{
	auto stats = whip::renderer2D::get_stats();
	ImGui::Begin("Settings");
	ImGui::ColorEdit4("Square Color", glm::value_ptr(m_square_color));
	ImGui::ColorEdit4("Second item Color", glm::value_ptr(m_second_color));
	ImGui::Text("Draw Calls: %d", stats.draw_calls);
	ImGui::Text("Quads: %d", stats.quad_counts);
	ImGui::Text("Vertices: %d", stats.get_total_vertex_count());
	ImGui::Text("Indices: %d", stats.get_total_index_count());
	ImGui::End();
}

void fbox_app2D::on_event(whip::event& evnt)
{
	m_camera_controller.on_event(evnt);
}