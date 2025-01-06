#include "F_boxApp2D.h"
#include <Whip/Core/EntryPoint.h>

#include <imgui/imgui.h>
#include <glm/gtc/type_ptr.hpp>

fbox_app2D::fbox_app2D() : layer("Fbox2D"), m_camera_controller((1280.0f / 720.0f), true) {}


void fbox_app2D::on_attach()
{
	WHP_PROFILE_FUNCTION();

	m_camera_controller.set_camera_position({ 0.0f, 0.0f, 0.0f });
	m_camera_controller.set_zoom_level(5.0f);
	m_camera_controller.set_camera_translation_speed(1.0f);

    //m_texture = whip::texture2D::create("C:\\Dev\\Whip Engine\\F-Box\\assets\\textures\\checkerboard.jpg");

    whip::framebuffer_specification fb_spec {};

    fb_spec.width = whip::application::get().get_window().get_width();
    fb_spec.height = whip::application::get().get_window().get_height();
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
	
	tm = ts.get_milliseconds();

	{
		WHP_PROFILE_SCOPE("renderer draw");
		whip::renderer2D::begin_scene(m_camera_controller.get_camera());
		whip::renderer2D::draw_quad({ 0, 0, 0 }, { 500.0f, 500.0f }, m_square_color);
		whip::renderer2D::draw_quad({ 0, 0, 0.1 }, { 5.0f, 5.0f }, m_texture);
		whip::renderer2D::end_scene();
	}
}

_WHP_PRAGMA_WARNING(push)
_WHP_PRAGMA_WARNING_DISABLE(4312)
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
_WHP_PRAGMA_WARNING(pop)

void fbox_app2D::on_event(whip::event& evnt)
{
	m_camera_controller.on_event(evnt);
}
