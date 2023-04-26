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

	m_test_texture = whip::texture2D::create("C:\\Dev\\Whip\\F-Box\\assets\\textures\\checkerboard.jpg");
	//m_test_texture2 = whip::texture2D::create("C:\\Dev\\Whip\\F-Box\\assets\\textures\\dirt_0.png");
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

	{
		WHP_PROFILE_SCOPE("renderer prep");
		whip::render_command::set_clear_color({ 0.1f, 0.1f, 0.1f, 1.0f });
		whip::render_command::clear();
	}
	

	{
		WHP_PROFILE_SCOPE("renderer draw");
		whip::renderer2D::begin_scene(m_camera_controller.get_camera());

		whip::renderer2D::draw_quad({ -1.0f, 0.0f }, { 0.9f, 0.9f }, m_square_color);
		whip::renderer2D::draw_quad({ 0.5f, -0.5f }, { 1.2f, 0.7f }, m_second_color);

		whip::renderer2D::draw_quad({ 0.0f, 0.0f, -0.1 }, { 100.0f, 100.0f }, m_test_texture);
		whip::renderer2D::end_scene();
	}
}

void fbox_app2D::on_imgui_render()
{
	ImGui::Begin("Color Settings");
	ImGui::ColorEdit4("Square Color", glm::value_ptr(m_square_color));
	ImGui::ColorEdit4("Second item Color", glm::value_ptr(m_second_color));
	ImGui::End();
}

void fbox_app2D::on_event(whip::Event& evnt)
{
	m_camera_controller.on_event(evnt);
}
