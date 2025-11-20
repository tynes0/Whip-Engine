#pragma once

#include <Whip.h>

class fbox_app2D : public whip::layer
{
public:
	fbox_app2D();
	virtual ~fbox_app2D() = default;

	virtual void on_attach() override;
	virtual void on_detach() override;
	virtual void on_update(whip::timestep ts) override;
	virtual void on_imgui_render() override;
	virtual void on_event(whip::event& evnt) override;

private:
	bool load_project(const std::filesystem::path& project_path);
	void load_start_scene();
	
	// Scene
	whip::ref<whip::scene> m_runtime_scene;
	whip::ref<whip::framebuffer> m_framebuffer;
	
	// Viewport
	glm::vec2 m_viewport_size = { 1280.0f, 720.0f };
	bool m_scene_loaded = false;
};

