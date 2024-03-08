#include "F_boxApp2D.h"
#include "Whip/Core/EntryPoint.h"

#include <imgui/imgui.h>
#include <glm/gtc/type_ptr.hpp>

static const uint32_t s_test_map_w = 20;
static int s_test_map[]
{
	81, 82, 83, -1, 81, 83, -1, -1, -1, 81, 82, 83, -1, -1, -1, -1, -1, -1, -1, -1,
	65, 66, 67, -1, 65, 67, -1, -1, -1, 65, 66, 67, 59, -1, -1, -1, -1, -1, -1, -1,
	49, 50, 51, -1, 49, 51, -1, -1, -1, 49, 50, 51, 28, 13, 13, 13, 13, 13, 13, 13
};

fbox_app2D::fbox_app2D()
	:layer("Fbox2D"), m_camera_controller(whip::calculate_aspect_ratio(1280, 720), true)
{
}

void fbox_app2D::on_attach()
{
	WHP_PROFILE_FUNCTION();

	m_camera_controller.set_camera_position({ -2.0f, 4.1f, 0.0f });
	m_camera_controller.set_zoom_level(5.0f);
	m_camera_controller.set_camera_translation_speed(1.0f);

	m_sprite_sheet = whip::texture2D::create("assets\\game\\textures\\tilemap.png");

	m_chill_character_frame = whip::texture2D::create("C:\\Dev\\Whip\\F-Box\\assets\\game\\textures\\character\\frame_1.png");

	glm::vec2 pixel_diff = { 1.0f, 1.0f };
	glm::vec2 sprite_size = { 18.0f, 18.0f };
	uint32_t vertical_count = static_cast<uint32_t>((m_sprite_sheet->get_height() + pixel_diff.y) / (sprite_size.y + pixel_diff.y));
	uint32_t horizontal_count = static_cast<uint32_t>((m_sprite_sheet->get_width() + pixel_diff.x) / (sprite_size.x + pixel_diff.x));
	int idx = 0;
	for (auto y : whip::range(vertical_count))
		for (auto x : whip::range(horizontal_count))
			m_texture_map[idx++] = whip::sub_texture2D::create_from_coords(m_sprite_sheet, { x, y }, sprite_size, pixel_diff);

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
		/*static int a = 250;
		if (a++ == 250)
		{
			a = 0;
		}*/
		tm = ts.get_milliseconds();
	}

	{
		WHP_PROFILE_SCOPE("renderer draw");


		whip::renderer2D::begin_scene(m_camera_controller.get_camera());
		
		whip::renderer2D::draw_quad({ 0, 0, 0 }, { 500.0f, 500.0f }, m_square_color);

		for (auto y : whip::range(100))
		{
			for (auto x : whip::range(3000))
			{
				whip::renderer2D::draw_quad({ x, y, 1.0f }, { 1.0f, 1.0f }, m_chill_character_frame);
			}
		}

		whip::renderer2D::end_scene();
	}
}

void fbox_app2D::on_imgui_render()
{
	auto stats = whip::renderer2D::get_stats();
	ImGui::Begin("Settings");
	ImGui::ColorEdit4("Square Color", glm::value_ptr(m_square_color));
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