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
	whip::orthographic_camera_controller m_camera_controller;
	float tm = 0;
	glm::vec4 m_square_color = { 0.2f, 0.4f, 0.2f, 1.0f };

	whip::ref<whip::texture2D> m_texture;
};

