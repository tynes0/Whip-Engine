#pragma once

#include <Whip/Render/Camera.h>

#include <Whip/Core/Timestep.h>

#include <Whip/Events/ApplicationEvent.h>
#include <Whip/Events/MouseEvent.h>

#include <Whip/Core/Input.h>

_WHIP_START

class orthographic_camera_controller
{
private:
	float m_aspect_ratio;
	float m_zoom_level = 1.0f;

	orthographic_camera m_camera;

	bool m_rotation;

	glm::vec3 m_camera_position = { 0.0f, 0.0f, 0.0f };
	float m_camera_rotation = 0.0f;

	float m_camera_translation_speed = 2.0f;
	float m_camera_rotation_speed = 120.0f;

	struct camera_movement_buttons
	{
		int camera_to_left, camera_to_right, camera_to_down, camera_to_up;
		int camera_rotation_left, camera_rotation_right;
		camera_movement_buttons() : camera_to_left(WHP_KEY_A), camera_to_right(WHP_KEY_D), camera_to_down(WHP_KEY_S), camera_to_up(WHP_KEY_W), camera_rotation_left(WHP_KEY_Q), camera_rotation_right(WHP_KEY_E) {}
		camera_movement_buttons(int ctl, int ctr, int ctd, int ctu, int crl, int crr) : camera_to_left(ctl), camera_to_right(ctr), camera_to_down(ctd), camera_to_up(ctu), camera_rotation_left(crl), camera_rotation_right(crr) {}
	};
	camera_movement_buttons buttons;
private:
	bool on_mouse_scrolled(mouse_scrolled_event& event);
	bool on_window_resized(window_resize_event& event);
public:
	orthographic_camera_controller(float aspect_ratio, bool rotation = false);

	void on_update(timestep ts);
	void on_event(Event& event);
	
	float get_zoom_level() const { return m_zoom_level; }
	bool is_rotatible() const { m_rotation; }
	float get_camera_rotation() const { return m_camera_rotation; }
	float get_camera_translation_speed() const { return m_camera_translation_speed; }
	float get_camera_rotation_speed() const { return m_camera_rotation_speed; }

	void set_zoom_level(float zoom_level) { m_zoom_level = zoom_level; }
	void set_rotatability(bool rotation) { m_rotation = rotation; }
	void set_camera_rotation(float rotation) { m_camera_rotation = rotation; }
	void set_camera_translation_speed(float translation_speed) { m_camera_translation_speed = translation_speed; }
	void set_camera_rotation_speed(float rotation_speed) { m_camera_rotation_speed = rotation_speed; }
	
	orthographic_camera& get_camera() { return m_camera; }
	const orthographic_camera& get_camera() const { return m_camera; }
};

_WHIP_END