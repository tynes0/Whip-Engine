#include "editor_layer.h"

#include <Whip/Core/EntryPoint.h>
#include <Whip/Scene/scene_serializer.h>
#include <Whip/Scripting/script_engine.h>
#include <Whip/Utils/platform_utils.h>
#include <Whip/UI/UI_helpers.h>
#include <Whip/UI/UI_project_loader.h>
#include <Whip/Math/math.h>
#include <Whip/Render/font.h>
#include <Whip/Asset/asset_manager.h>
#include <Whip/Asset/texture_importer.h>
#include <Whip/Asset/scene_importer.h>

#include <array>

#include <imgui/imgui.h>
#include <glm/gtc/type_ptr.hpp>
#include <entt.hpp>
#include <ImGuizmo/ImGuizmo.h>

_WHIP_START


editor_layer::editor_layer() : layer("Fbox2D") 
{
}


void editor_layer::on_attach()
{
    WHP_PROFILE_FUNCTION();

	// icons
	m_play_icon		= texture_importer::load_texture2D("resources/icons/play_icon.png");
	m_simulate_icon = texture_importer::load_texture2D("resources/icons/simulate_icon.png");
	m_stop_icon		= texture_importer::load_texture2D("resources/icons/stop_icon.png");
	m_pause_icon	= texture_importer::load_texture2D("resources/icons/pause_icon.png");
	m_step_icon		= texture_importer::load_texture2D("resources/icons/step_icon.png");

	ref<texture2D> flipped_step_icon = texture_importer::load_texture2D("resources/icons/step_icon.png", flip_direction_horizontal);

	m_animation_editor_panel.load_icon(animation_editor_panel::icon::play, m_play_icon);
	m_animation_editor_panel.load_icon(animation_editor_panel::icon::pause, m_pause_icon);
	m_animation_editor_panel.load_icon(animation_editor_panel::icon::stop, m_stop_icon);
	m_animation_editor_panel.load_icon(animation_editor_panel::icon::to_first_frame, flipped_step_icon); 
	m_animation_editor_panel.load_icon(animation_editor_panel::icon::to_last_frame, m_step_icon);

	m_animation_editor_panel.set_refresh_asset_tree_callback([this]() {if (m_content_browser_panel) { m_content_browser_panel->refresh_asset_tree(); } });


	// framebuffer
    framebuffer_specification fb_spec{};
    fb_spec.attachments = { framebuffer_texture_format::RGBA8, framebuffer_texture_format::RED_INTEGER, framebuffer_texture_format::depth };
    fb_spec.width = application::get().get_window().get_width();
    fb_spec.height = application::get().get_window().get_height();
    m_framebuffer = framebuffer::create(fb_spec);

	// scene
    m_editor_scene = make_ref<scene>();
	m_active_scene = m_editor_scene;

	// project
	auto command_line_args = application::get().get_specification().command_line_args;
	if (command_line_args.count > 1)
	{
		auto project_filepath = command_line_args[1];
		open_project(project_filepath);
	}
	else
	{
		if (!open_project())
			application::get().close();
	}
	// camera
    m_editor_camera = editor_camera(30.0f, 1.778f, 0.1f, 1000.0f);
	m_console.initialize();
	static float v1 = 0, v2 = 0;
	m_popup_handler
		.set_popup_name("Popup Testing")
		.set_height(300.f)
		.set_width(400.f)
		.add([]() { ImGui::Text("This is some text message for popup testing. Do not mind this window if you see that."); })
		.add([]() { static float fv = 0; ImGui::SliderFloat("##Float value", &fv, 0.0f, 10000.0f); })
		.same_line()
		.add([]() { static int iv = 0; ImGui::SliderInt("##Int value", &iv, 0, 1000000); })
		.add_dual_handle_slider(0, 100, &v1, &v2)
		.add_button([this]() { m_popup_handler.set_show_state(false); }, "Close", 100);
}

void editor_layer::on_detach()
{
	WHP_PROFILE_FUNCTION();
	m_console.shutdown();
}

void editor_layer::on_update(timestep ts)
{
	WHP_PROFILE_FUNCTION();
	m_ts = ts;

	{
		WHP_PROFILE_SCOPE("Viewport Size");
		m_active_scene->on_viewport_resize((uint32_t)m_viewport_size.x, (uint32_t)m_viewport_size.y);
		if (framebuffer_specification spec = m_framebuffer->get_specification();
			m_viewport_size.x > 0.0f && m_viewport_size.y > 0.0f && (spec.width != m_viewport_size.x || spec.height != m_viewport_size.y))
		{
			m_framebuffer->resize((uint32_t)m_viewport_size.x, (uint32_t)m_viewport_size.y);
			m_editor_camera.set_viewport_size(m_viewport_size.x, m_viewport_size.y);
		}
	}

	{
		WHP_PROFILE_SCOPE("scene::on_update");
		renderer2D::reset_stats();
		m_framebuffer->bind();
		render_command::set_clear_color({ 0.1f, 0.1f, 0.1f, 1.0f });
		render_command::clear();

		m_framebuffer->clear_attachment(1, -1);

		switch (m_scane_state)
		{
		case scane_state::edit:
		{
			m_editor_camera.on_update(ts);

			m_active_scene->on_update_editor(ts, m_editor_camera);
			break;
		}
		case scane_state::play:
		{
			m_active_scene->on_update_runtime(ts);
			break;
		}
		case scane_state::simulate:
		{
			m_editor_camera.on_update(ts);
			m_active_scene->on_update_simulation(ts, m_editor_camera);
			break;
		}
		}
	}

	{
		WHP_PROFILE_SCOPE("Mouse position track");
		auto [mx, my] = ImGui::GetMousePos();
		mx -= m_viewport_bounds[0].x;
		my -= m_viewport_bounds[0].y;
		glm::vec2 viewport_size = m_viewport_bounds[1] - m_viewport_bounds[0];
		my = viewport_size.y - my;
		int mouseX = (int)mx;
		int mouseY = (int)my;

		if (mouseX >= 0 && mouseY >= 0 && mouseX < (int)viewport_size.x && mouseY < (int)viewport_size.y)
		{
			int pixel_data = m_framebuffer->read_pixel(1, mouseX, mouseY); // This is taking too much time
			m_hovered_entity = pixel_data == -1 ? entity() : entity((entt::entity)pixel_data, m_active_scene.get());
		}
	}

	on_overlay_render();

    m_framebuffer->unbind();
}

_WHP_PRAGMA_WARNING(push)
_WHP_PRAGMA_WARNING_DISABLE(4312)
void editor_layer::on_imgui_render()
{
	WHP_PROFILE_FUNCTION();
	// dockspace
	{
		static bool p_open = true;
		static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

		ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;

		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->WorkPos);
		ImGui::SetNextWindowSize(viewport->WorkSize);
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
		window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

		if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
			window_flags |= ImGuiWindowFlags_NoBackground;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("Editor DockSpace", &p_open, window_flags);
		ImGui::PopStyleVar(3);

		ImGuiIO& io = ImGui::GetIO();
		ImGuiStyle& style = ImGui::GetStyle();
		float minWinSizeX = style.WindowMinSize.x;
		style.WindowMinSize.x = 300.0f;
		if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
		{
			ImGuiID dockspace_id = ImGui::GetID("Editor DockSpace");
			ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
		}
		style.WindowMinSize.x = minWinSizeX;
	}
	// menu bar
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Open Project", "Ctrl+O"))
                open_project();
			if (ImGui::MenuItem("Save Project", "Ctrl+Shift+S"))
				save_project();
			ImGui::Separator();
            if (ImGui::MenuItem("New Scene", "Ctrl+N"))
                new_scene();
			if (ImGui::MenuItem("Save Scene", "Ctrl+S"))
				save_scene();
            if (ImGui::MenuItem("Save Scane As...", "Ctrl+Alt+S"))
                save_scene_as();
			ImGui::Separator();
            if (ImGui::MenuItem("Restart"))
                application::get().restart();
			if (ImGui::MenuItem("Exit"))
				application::get().close();
            ImGui::EndMenu();
        }
		if (ImGui::BeginMenu("Edit"))
		{
			if (ImGui::MenuItem("Show Settings Window"))
				m_UI_settings.open_window();
			if (ImGui::MenuItem("Show Animation Editor"))
				m_animation_editor_panel.open();
			if (ImGui::MenuItem("Show Test Popup"))
				m_popup_handler.set_show_state(true);
			
			static float level = 1.0f;
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Script"))
		{
			if (ImGui::MenuItem("Reload assembly", "Ctrl+R"))
				reload_assembly(true);
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Project"))
		{
			if (ImGui::MenuItem("Settings"))
				m_UI_project.show(UI::UI_project::UI_settings, [this]() -> decltype(auto) { return this->finish_project_settings(); });
			ImGui::Separator();
			if (ImGui::MenuItem("Open Project", "Ctrl+O"))
				open_project();
			if (ImGui::MenuItem("Save Project", "Ctrl+Shift+S"))
				save_project();
			ImGui::EndMenu();
		}

        ImGui::EndMenuBar();
    }
	// viewport
	{
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0.0f, 0.0f });
		ImGui::Begin("Viewport");
		auto viewport_min_region = ImGui::GetWindowContentRegionMin();
		auto viewport_max_region = ImGui::GetWindowContentRegionMax();
		auto viewport_offset = ImGui::GetWindowPos();
		m_viewport_bounds[0] = { viewport_min_region.x + viewport_offset.x, viewport_min_region.y + viewport_offset.y };
		m_viewport_bounds[1] = { viewport_max_region.x + viewport_offset.x, viewport_max_region.y + viewport_offset.y };
		m_viewport_focused = ImGui::IsWindowFocused();
		m_viewport_hovered = ImGui::IsWindowHovered();
		application::get().get_imgui_layer()->block_events(!m_viewport_hovered);
		ImVec2 viewport_panel_size = ImGui::GetContentRegionAvail();
		m_viewport_size = { viewport_panel_size.x, viewport_panel_size.y };

		ImGui::Image(reinterpret_cast<void*>(m_framebuffer->get_color_attachment_renderer_id()), viewport_panel_size, ImVec2{ 0.0f, 1.0f }, ImVec2{ 1.0f, 0.0f });
		UI::drag_drop_target(asset_type::scene, [this](asset_handle handle) { open_scene(handle); }, "scene drag drop", false);

		// gizmos
		entity selected_entity = m_scene_hierarchy_panel.get_selected_entity();
		if (selected_entity && m_gizmo_type != -1 && m_scane_state != scane_state::play)
		{
		    ImGuizmo::SetDrawlist();
		    ImGuizmo::SetRect(m_viewport_bounds[0].x, m_viewport_bounds[0].y, m_viewport_bounds[1].x - m_viewport_bounds[0].x, m_viewport_bounds[1].y - m_viewport_bounds[0].y);
		    // Camera
		    const glm::mat4& camera_projection = m_editor_camera.get_projection();
		    glm::mat4 camera_view = m_editor_camera.get_view_matrix();

		    // Entity transform
		    auto& tc = selected_entity.get_component<transform_component>();
		    glm::mat4 transform = tc.get_transform();

		    // Snapping
		    bool snap = input::is_key_down(key::left_control);

			ImGuizmo::Manipulate(
				glm::value_ptr(camera_view), 
				glm::value_ptr(camera_projection), 
				(ImGuizmo::OPERATION)m_gizmo_type, 
				ImGuizmo::LOCAL, 
				glm::value_ptr(transform), 
				nullptr, 
				snap ? const_cast<float*>(glm::value_ptr(m_UI_settings.get_snap_values(m_gizmo_type))) : nullptr);

		    if (ImGuizmo::IsUsing())
		    {
		        glm::vec3 translation, rotation, scale;
				if (!math::decompose_transform(transform, translation, rotation, scale))
					WHP_CLIENT_WARN("Transform Decomposing error!");
		        glm::vec3 deltaRotation = rotation - tc.rotation;
		        tc.translation = translation;
		        tc.rotation += deltaRotation;
		        tc.scale = scale;
		    }
		}
		ImGui::End();
		ImGui::PopStyleVar();
	} // viewport 

	m_UI_project.on_imgui_render(); // should be in dockspace

    ImGui::End(); // dockspace

	// other renders
	UI_toolbar();
	m_UI_statistics.on_imgui_render(m_ts);
	m_UI_settings.on_imgui_render();
    m_scene_hierarchy_panel.on_imgui_render();
    m_animation_editor_panel.on_imgui_render();
	m_console.render_imgui_console();
	m_content_browser_panel->on_imgui_render();
	m_popup_handler.on_imgui_render();
	
}
_WHP_PRAGMA_WARNING(pop)

void editor_layer::on_event(event& evnt)
{
	if(m_scane_state == scane_state::edit)
		m_editor_camera.on_event(evnt);
    event_dispatcher dispatcher(evnt);
    dispatcher.dispatch<key_pressed_event>([this](auto&&... args) -> decltype(auto) { return this->on_key_pressed(std::forward<decltype(args)>(args)...); });
    dispatcher.dispatch<mouse_button_pressed_event>([this](auto&&... args) -> decltype(auto) { return this->on_mouse_button_pressed(std::forward<decltype(args)>(args)...); });
}

bool editor_layer::on_key_pressed(key_pressed_event& evnt)
{
    // Shortcuts
    if (evnt.get_repeat_count() > 0)
        return false;
    bool control = input::is_key_down(key::left_control) || input::is_key_down(key::right_control);
    bool shift = input::is_key_down(key::left_shift) || input::is_key_down(key::right_shift);
    bool alt = input::is_key_down(key::left_alt) || input::is_key_down(key::right_alt);
    switch (evnt.get_keycode())
    {
    case key::N:
    {
		if (control)
			new_scene();
		WHP_EDITOR_TRACE("Trace Test");
		WHP_EDITOR_DEBUG("Debug Test");
		WHP_EDITOR_INFO("Info Test");
		WHP_EDITOR_WARN("Warn Test");
		WHP_EDITOR_ERROR("Error Test");
		WHP_EDITOR_CRITICAL("Critical Test");
        break;
    }
    case key::O:
    {
        if (control)
            open_project();
        break;
    }
    case key::S:
    {
		if (control)
		{
			if (shift)
				save_project();
			if (alt)
				save_scene_as();
			else
				save_scene();
		}
        break;
    }
	case key::P:
		if (control && m_scane_state != scane_state::simulate)
			m_scane_state = scane_state::play;
        break;
	case key::escape:
		if(m_scane_state == scane_state::play || m_scane_state == scane_state::simulate)
			on_scene_stop();
		break;
	case key::D:
	{
		if (control)
			on_duplicated_entity();
		break;
	}
    case key::Q:
	{
		if (!ImGuizmo::IsUsing())
			m_gizmo_type = -1;
		break;
	}
    case key::W:
	{
		if (control)
		{
			close_scene();
			break;
		}
		if (!ImGuizmo::IsUsing())
			m_gizmo_type = ImGuizmo::OPERATION::TRANSLATE;
		break;
	}
    case key::E:
	{
		if (!ImGuizmo::IsUsing())
			m_gizmo_type = ImGuizmo::OPERATION::ROTATE;
		break;
	}
    case key::R:
	{
		if (control)
			reload_assembly(true);
		else
			if (!ImGuizmo::IsUsing())
				m_gizmo_type = ImGuizmo::OPERATION::SCALE;
		break;
	}
	case key::del:
	{
		on_deleted_entity();
		break;
	}
    }
    return true;
}

bool editor_layer::on_mouse_button_pressed(mouse_button_pressed_event& evnt)
{
    if (evnt.get_mouse_button() == mouse::button_left)
    {   
        if (m_viewport_hovered && !ImGuizmo::IsOver() && !input::is_key_down(key::left_alt))
            m_scene_hierarchy_panel.set_selected_entity(m_hovered_entity);
    }
    return false;
}

bool editor_layer::on_window_drop(window_drop_event& evnt)
{
	return true;
}

void editor_layer::on_overlay_render()
{
	WHP_PROFILE_FUNCTION();
	if (m_scane_state == scane_state::play)
	{
		entity cam = m_active_scene->get_primary_camera_entity();
		if (!cam)
			return;
		renderer2D::begin_scene(cam.get_component<camera_component>().camera, cam.get_component<transform_component>().get_transform());
	}
	else
	{
		renderer2D::begin_scene(m_editor_camera);
	}

	if (m_UI_settings.get_show_physics_colliders())
	{
		// Box Colliders
		{
			auto view = m_active_scene->get_all_entities_with<transform_component, box_collider2D_component>();
			for (auto entity : view)
			{
				auto [tc, bc2d] = view.get<transform_component, box_collider2D_component>(entity);

				glm::vec3 translation = tc.translation + glm::vec3(bc2d.offset, 0.001f);
				glm::vec3 scale = tc.scale * glm::vec3(bc2d.size * 2.0f, 1.0f);

				glm::mat4 transform = glm::translate(glm::mat4(1.0f), tc.translation)
					* glm::rotate(glm::mat4(1.0f), tc.rotation.z, glm::vec3(0.0f, 0.0f, 1.0f))
					* glm::translate(glm::mat4(1.0f), glm::vec3(bc2d.offset, 0.001f))
					* glm::scale(glm::mat4(1.0f), scale);

				renderer2D::draw_rect(transform, glm::vec4(0, 1, 0, 1));
			}
		}

		// Circle Colliders
		{
			auto view = m_active_scene->get_all_entities_with<transform_component, circle_collider2D_component>();
			for (auto entity : view)
			{
				auto [tc, cc2d] = view.get<transform_component, circle_collider2D_component>(entity);

				glm::vec3 translation = tc.translation + glm::vec3(cc2d.offset, 0.001f);
				glm::vec3 scale = tc.scale * glm::vec3(cc2d.radius * 2.0f);

				glm::mat4 transform = glm::translate(glm::mat4(1.0f), translation)
					* glm::scale(glm::mat4(1.0f), scale);

				renderer2D::draw_circle(transform, glm::vec4(0, 1, 0, 1), 0.01f);
			}
		}
	}

	if (entity selected_entity = m_scene_hierarchy_panel.get_selected_entity())
	{
		transform_component transform = selected_entity.get_component<transform_component>();
		if (selected_entity.has_component<text_component>() && !selected_entity.has_component<sprite_renderer_component>() && !selected_entity.has_component<circle_renderer_component>())
		{
		}
		else 
			renderer2D::draw_rect(transform.get_transform(), glm::vec4(0.9f, 0.4f, 0.1f, 1.0f));
	}

	renderer2D::end_scene();
}

void editor_layer::new_project()
{
	close_scene();
	save_project();
}

void editor_layer::save_project()
{
	project::save_active();
}

void editor_layer::finish_project_settings()
{
	project::save_active();
	reload_assembly(true);
	m_content_browser_panel = make_scope<content_browser_panel>(project::get_active());
}


bool editor_layer::open_project()
{
	std::string filepath = file_dialogs::open_file("Whip Project (*.wproj)\0*.wproj\0");
	if (filepath.empty())
		return false;
	return open_project(filepath);
}

bool editor_layer::open_project(const std::filesystem::path& path)
{
	if (project::load(path))
	{
		script_engine::init();
		asset_handle start_scene = (project::get_active()->get_config().start_scene);
		if(start_scene)
			open_scene(start_scene);
		m_content_browser_panel = make_scope<content_browser_panel>(project::get_active());
		return true;
	}
	return false;
}

void editor_layer::new_scene()
{
    m_active_scene = make_ref<scene>();
	m_scene_hierarchy_panel.set_context(m_active_scene);
	m_editor_scene_path = std::filesystem::path();
}

void editor_layer::open_scene(asset_handle handle)
{
	if (m_scane_state != scane_state::edit)
		on_scene_stop();
	
	ref<scene> read_only_scene = asset_manager::get_asset<scene>(handle);
	ref<scene> new_scene = scene::copy(read_only_scene);

	m_editor_scene = new_scene;
	m_scene_hierarchy_panel.set_context(m_editor_scene);

	m_active_scene = m_editor_scene;
	m_editor_scene_path = project::get_active()->get_editor_asset_manager()->get_filepath(handle);
}

void editor_layer::close_scene()
{
	if (m_scane_state != scane_state::edit)
		on_scene_stop();
	ref<scene> new_scene = make_ref<scene>();
	m_editor_scene = new_scene;
	m_editor_scene->on_viewport_resize((uint32_t)m_viewport_size.x, (uint32_t)m_viewport_size.y);
	m_active_scene = m_editor_scene;
	m_scene_hierarchy_panel.set_context({});
}

void editor_layer::save_scene()
{
	if (!m_editor_scene_path.empty())
		serialize_scene(m_active_scene, m_editor_scene_path);
	else
		save_scene_as();
}

void editor_layer::save_scene_as()
{
    std::string filepath = file_dialogs::save_file("Whip Scene (*.whip)\0*.whip\0");
    if (!filepath.empty())
    {
		serialize_scene(m_active_scene, filepath);
		m_editor_scene_path = filepath;
    }
}

void editor_layer::reload_assembly(bool reset_app_assembly_filepath) const
{
	if (m_scane_state == scane_state::edit)
		assembly_manager::reload_assembly(reset_app_assembly_filepath);
	else
		WHP_CORE_WARN("[Script Engine] Failed to reload assembly. Scene is running or simulating!");
}

void editor_layer::serialize_scene(ref<scene> scene_in, const std::filesystem::path& path)
{
	scene_importer::save_scene(scene_in, path);
}

void editor_layer::on_scene_play()
{
	if (m_scane_state == scane_state::simulate)
		on_scene_stop();
	project::run_state(true);
	m_scane_state = scane_state::play;
	script_engine::set_filewatcher_state(false);
	m_active_scene = scene::copy(m_editor_scene);
	m_active_scene->on_runtime_start();
	m_last_selected_entity = m_scene_hierarchy_panel.get_selected_entity();
	m_scene_hierarchy_panel.set_context(m_active_scene);
}

void editor_layer::on_scene_simulate()
{
	if (m_scane_state == scane_state::play)
		on_scene_stop();

	project::run_state(true);
	m_scane_state = scane_state::simulate;
	script_engine::set_filewatcher_state(false);
	m_active_scene = scene::copy(m_editor_scene);
	m_active_scene->on_simulation_start();
	
	m_active_scene->on_runtime_start();
	m_last_selected_entity = m_scene_hierarchy_panel.get_selected_entity();
	m_scene_hierarchy_panel.set_context(m_active_scene);
	// maybe do not ??
	if(m_last_selected_entity)
		m_scene_hierarchy_panel.set_selected_entity(m_active_scene->find_entity_by_UUID(m_last_selected_entity.get_UUID()));
}

void editor_layer::on_scene_stop()
{
	WHP_CORE_ASSERT(m_scane_state == scane_state::play || m_scane_state == scane_state::simulate, "invalid scane_state!");
	project::run_state(false);
	if (m_scane_state == scane_state::play)
		m_active_scene->on_runtime_stop();
	else if (m_scane_state == scane_state::simulate)
		m_active_scene->on_simulation_stop();
	m_scane_state = scane_state::edit;
	script_engine::set_filewatcher_state(true);
	m_active_scene = m_editor_scene;
	m_scene_hierarchy_panel.set_context(m_active_scene);
	m_scene_hierarchy_panel.set_selected_entity(m_last_selected_entity);
}

void editor_layer::on_scene_pause()
{

}

void editor_layer::on_duplicated_entity()
{
	if (m_scane_state != scane_state::edit)
		return;

	entity selected_entity = m_scene_hierarchy_panel.get_selected_entity();
	if (selected_entity)
		m_scene_hierarchy_panel.set_selected_entity(m_editor_scene->duplicate_entity(selected_entity));
}

void editor_layer::on_deleted_entity()
{
	if(application::get().get_imgui_layer()->get_active_widgetID() == 0)
	{
		entity selected_entity = m_scene_hierarchy_panel.get_selected_entity();
		if (selected_entity)
		{
			m_scene_hierarchy_panel.set_selected_entity({});
			m_active_scene->destroy_entity(selected_entity);
		}
	}
}

void editor_layer::UI_toolbar()
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 2));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(0, 0));
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
	auto& colors = ImGui::GetStyle().Colors;
	const auto& buttonHovered = colors[ImGuiCol_ButtonHovered];
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(buttonHovered.x, buttonHovered.y, buttonHovered.z, 0.5f));
	const auto& buttonActive = colors[ImGuiCol_ButtonActive];
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(buttonActive.x, buttonActive.y, buttonActive.z, 0.5f));

	ImGui::Begin("##toolbar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

	bool toolbar_enabled = (bool)m_scene_hierarchy_panel.get_context();

	ImVec4 tint_color = ImVec4(1, 1, 1, 1);
	if (!toolbar_enabled)
		tint_color.w = 0.5f;

	float size = ImGui::GetWindowHeight() - 4.0f;
	ImGui::SetCursorPosX((ImGui::GetWindowContentRegionMax().x * 0.5f) - (size * 0.5f));

	bool has_play_button = m_scane_state == scane_state::edit|| m_scane_state == scane_state::play;
	bool has_simulate_button = m_scane_state == scane_state::edit || m_scane_state == scane_state::simulate;
	bool has_pause_button = m_scane_state != scane_state::edit;

	if(has_play_button)
	{
		ref<texture2D> icon = (m_scane_state == scane_state::edit || m_scane_state == scane_state::simulate) ? m_play_icon : m_stop_icon;
		ImGui::SetCursorPosX((ImGui::GetWindowContentRegionMax().x * 0.5f) - (size * 0.5f) - (size / 2));
		if (ImGui::ImageButton(reinterpret_cast<ImTextureID>((uint64_t)icon->get_renderer_id()), ImVec2(size, size), ImVec2(0, 1), ImVec2(1, 0), 0, ImVec4(0.0f, 0.0f, 0.0f, 0.0f), tint_color) && toolbar_enabled)
		{
			if (m_scane_state == scane_state::edit || m_scane_state == scane_state::simulate)
				on_scene_play();
			else if (m_scane_state == scane_state::play)
				on_scene_stop();
		}
	}
	if(has_simulate_button)
	{
		if(has_play_button)
			ImGui::SameLine(0, 20);
		ref<texture2D> icon = (m_scane_state == scane_state::edit || m_scane_state == scane_state::play) ? m_simulate_icon : m_stop_icon;
		if (ImGui::ImageButton(reinterpret_cast<ImTextureID>((uint64_t)icon->get_renderer_id()), ImVec2(size, size), ImVec2(0, 1), ImVec2(1, 0), 0, ImVec4(0.0f, 0.0f, 0.0f, 0.0f), tint_color) && toolbar_enabled)
		{
			if (m_scane_state == scane_state::edit || m_scane_state == scane_state::play)
				on_scene_simulate();
			else if (m_scane_state == scane_state::simulate)
				on_scene_stop();
		}
	}
	if (has_pause_button)
	{
		bool is_paused = m_active_scene->is_paused();
		ImGui::SameLine();
		{
			ref<texture2D> icon = m_pause_icon;
			if (ImGui::ImageButton(reinterpret_cast<ImTextureID>((uint64_t)icon->get_renderer_id()), ImVec2(size, size), ImVec2(0, 1), ImVec2(1, 0), 0, ImVec4(0.0f, 0.0f, 0.0f, 0.0f), tint_color) && toolbar_enabled)
			{
				m_active_scene->set_paused(!is_paused);
			}
		}

		// Step button
		if (is_paused)
		{
			ImGui::SameLine(0, 20);
			{
				ref<texture2D> icon = m_step_icon;
				bool isPaused = m_active_scene->is_paused();
				if (ImGui::ImageButton(reinterpret_cast<ImTextureID>((uint64_t)icon->get_renderer_id()), ImVec2(size, size), ImVec2(0, 1), ImVec2(1, 0), 0, ImVec4(0.0f, 0.0f, 0.0f, 0.0f), tint_color) && toolbar_enabled)
				{
					m_active_scene->step(m_UI_settings.get_step_frame());
				}
			}
		}
	}
	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor(3);
	ImGui::End();
}

_WHIP_END
