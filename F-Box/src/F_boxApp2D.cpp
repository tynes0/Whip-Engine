#include "F_boxApp2D.h"
#include "Whip/Core/EntryPoint.h"

#include <imgui/imgui.h>
#include <glm/gtc/type_ptr.hpp>

fbox_app2D::fbox_app2D()
	:layer("Fbox2D"), m_camera_controller(whip::calculate_aspect_ratio(1280, 720), true)
{
}

void fbox_app2D::on_attach()
{
	WHP_PROFILE_FUNCTION();

	m_test_texture = whip::texture2D::create("assets\\textures\\checkerboard.jpg");
	m_sprite_sheet = whip::texture2D::create("assets\\game\\textures\\sprite.png");
	m_sprite = whip::sub_texture2D::create_from_coords(m_sprite_sheet, { 2.0f,1.0f }, { 128.0f,128.0f }, {1.0f,2.0f});

	m_particle.ColorBegin = { 1.0f, 0.0f, 0.4f, 1.0f };
	m_particle.ColorEnd = { 0.0f, 0.4f, 1.0f, 1.0f };
	m_particle.SizeBegin = 0.5f, m_particle.SizeVariation = 0.3f, m_particle.SizeEnd = 0.0f;
	m_particle.LifeTime = 5.0f;
	m_particle.Velocity = { 0.0f, 0.0f };
	m_particle.VelocityVariation = { 3.0f, 1.0f };
	m_particle.Position = { 0.0f, 0.0f };
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

	/*
	{
		WHP_PROFILE_SCOPE("renderer draw");

		static float rotation = 0.0f;
		rotation += ts * 45.0f;

		whip::renderer2D::begin_scene(m_camera_controller.get_camera());

		whip::renderer2D::draw_quad({ -1.0f, 0.0f }, { 0.9f, 0.9f }, m_square_color);
		whip::renderer2D::draw_rotated_quad({ 1.0f, -1.5f }, { 1.0f, 1.0f }, glm::radians(rotation), m_second_color);
		whip::renderer2D::draw_quad({ 0.5f, -0.5f }, { 1.2f, 0.7f }, m_second_color);

		whip::renderer2D::draw_quad({ 0.0f, 0.0f, -0.1 }, { 100.0f, 100.0f }, m_test_texture, 10.0f);
		//whip::renderer2D::draw_rotated_quad({ 0.0f, 0.0f }, { 1.0f, 1.0f }, rotation, m_test_texture, 2.0f);
		whip::renderer2D::end_scene();

		whip::renderer2D::begin_scene(m_camera_controller.get_camera());
		for (float y = -5.0f; y < 5.0f; y += 0.5f)
		{
			for (float x = -5.0f; x < 5.0f; x += 0.5f)
			{
				glm::vec4 color = { (x + 5.0f) / 10.0f, 0.4f, (y + 5.0f) / 10.0f, 0.7f };
				whip::renderer2D::draw_quad({ x, y }, { 0.45,0.45 }, color);
			}
		}
		whip::renderer2D::end_scene();
	}
*/

	if (whip::input::is_mouse_button_pressed(WHP_MOUSE_BUTTON_LEFT))
	{
		auto [x, y] = whip::input::get_mouse_position();
		auto width = whip::application::get().get_window().get_width();
		auto height = whip::application::get().get_window().get_height();

		auto bounds = m_camera_controller.get_bounds();
		glm::vec3 pos = m_camera_controller.get_camera().get_position();
		x = (x / width) * bounds.get_width() - bounds.get_width() * 0.5f;
		y = bounds.get_height() * 0.5f - (y / height) * bounds.get_height();
		m_particle.Position = { x + pos.x, y + pos.y };
		for (int i = 0; i < 50; i++)
			m_particle_system.Emit(m_particle);
	}

	m_particle_system.OnUpdate(ts);
	m_particle_system.OnRender(m_camera_controller.get_camera());

	whip::renderer2D::begin_scene(m_camera_controller.get_camera());

	whip::renderer2D::draw_quad({ 0.0f, 0.0f }, { 1.0f, 2.0f }, m_sprite);

	whip::renderer2D::end_scene();
}

void fbox_app2D::on_imgui_render()
{
	auto stats = whip::renderer2D::get_stats();

	ImGui::Begin("Renderer2D Stats:");
	ImGui::Text("Draw Calls: %d", stats.draw_calls);
	ImGui::Text("Quads: %d", stats.quad_counts);
	ImGui::Text("Vertices: %d", stats.get_total_vertex_count());
	ImGui::Text("Indices: %d", stats.get_total_index_count());
	ImGui::End();


	ImGui::Begin("Settings");
	ImGui::ColorEdit4("Square Color", glm::value_ptr(m_square_color));
	ImGui::ColorEdit4("Second item Color", glm::value_ptr(m_second_color));
	ImGui::End();
}

void fbox_app2D::on_event(whip::Event& evnt)
{
	m_camera_controller.on_event(evnt);
}