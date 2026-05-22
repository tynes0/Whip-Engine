#include "F_boxApp2D.h"
#include <Whip/Core/EntryPoint.h>
#include <Whip/Scene/scene_serializer.h>
#include <Whip/Scripting/script_engine.h>
#include <Whip/Asset/asset_manager.h>

#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>

fbox_app2D::fbox_app2D() : layer("Fbox2D") {}

void fbox_app2D::on_attach()
{
	whip::framebuffer_specification fb_spec{};
	fb_spec.attachments = { whip::framebuffer_texture_format::RGBA8, whip::framebuffer_texture_format::depth };
	fb_spec.width = (uint32_t)m_viewport_size.x;
	fb_spec.height = (uint32_t)m_viewport_size.y;
	m_framebuffer = whip::framebuffer::create(fb_spec);

	auto command_line_args = whip::application::get().get_specification().command_line_args;
	if (command_line_args.count > 1)
	{
		auto project_filepath = command_line_args[1];
		if (load_project(project_filepath))
		{
			load_start_scene();
		}
		else
		{
			WHP_CORE_ERROR("Failed to load project: {0}", project_filepath);
			whip::application::get().close();
		}
	}
	else
	{
		if (load_project(whip::file_dialogs::open_file("Whip Project (*.wproj)\0*.wproj\0")))
		{
			load_start_scene();
		}
		
	}
}

void fbox_app2D::on_detach()
{
	if (m_runtime_scene)
		m_runtime_scene->on_runtime_stop();
}

void fbox_app2D::on_update(whip::timestep ts)
{
	if (!m_scene_loaded || !m_runtime_scene)
		return;

	// Viewport resize
	if (whip::framebuffer_specification spec = m_framebuffer->get_specification();
		m_viewport_size.x > 0.0f && m_viewport_size.y > 0.0f && 
		(spec.width != m_viewport_size.x || spec.height != m_viewport_size.y))
	{
		m_framebuffer->resize((uint32_t)m_viewport_size.x, (uint32_t)m_viewport_size.y);
		m_runtime_scene->on_viewport_resize((uint32_t)m_viewport_size.x, (uint32_t)m_viewport_size.y);
	}

	// Render
	whip::renderer2D::reset_stats();
	m_framebuffer->bind();
	whip::render_command::set_clear_color({ 0.1f, 0.1f, 0.1f, 1.0f });
	whip::render_command::clear();

	// Runtime update
	m_runtime_scene->on_update_runtime(ts);

	m_framebuffer->unbind();
}

void fbox_app2D::on_imgui_render()
{
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | 
									 ImGuiWindowFlags_NoMove | 
									 ImGuiWindowFlags_NoResize | 
									 ImGuiWindowFlags_NoScrollWithMouse |
									 ImGuiWindowFlags_NoBringToFrontOnFocus;

	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);
	
	ImGui::Begin("Game Viewport", nullptr, window_flags);
	
	ImVec2 viewport_panel_size = ImGui::GetContentRegionAvail();
	m_viewport_size = { viewport_panel_size.x, viewport_panel_size.y };

	// Framebuffer'ı göster
	if (m_framebuffer)
	{
		ImGui::Image(
			reinterpret_cast<void*>(m_framebuffer->get_color_attachment_renderer_id()), 
			viewport_panel_size, 
			ImVec2{ 0.0f, 1.0f }, 
			ImVec2{ 1.0f, 0.0f }
		);
	}
	
	ImGui::End();
}

void fbox_app2D::on_event(whip::event& evnt)
{
	// Input eventlerini handle et
}

bool fbox_app2D::load_project(const std::filesystem::path& project_path)
{
	if (whip::project::load(project_path))
	{
		whip::script_engine::init();
		return true;
	}
	return false;
}

void fbox_app2D::load_start_scene()
{
	whip::asset_handle start_scene_handle = whip::project::get_active()->get_config().start_scene;
	
	if (!start_scene_handle)
	{
		WHP_CORE_ERROR("No start scene specified in project!");
		return;
	}

	whip::ref<whip::scene> loaded_scene = whip::asset_manager::get_asset<whip::scene>(start_scene_handle);
	m_runtime_scene = whip::scene::copy(loaded_scene);
	m_runtime_scene->on_viewport_resize((uint32_t)m_viewport_size.x, (uint32_t)m_viewport_size.y);
	
	m_runtime_scene->on_runtime_start();
	m_scene_loaded = true;
	
	WHP_CORE_INFO("Scene loaded and started!");
}
