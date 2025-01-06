#pragma once

#include <Whip/Render/orthographic_camera.h>

#include <Whip/Core/Timestep.h>

#include <Whip/Events/ApplicationEvent.h>
#include <Whip/Events/MouseEvent.h>

#include <Whip/Core/Input.h>

_WHIP_START

struct orthographic_camera_bounds
{
	float left, right, bottom, top;

	float get_width() { return right - left; }
	float get_height() { return top - bottom; }
};

class orthographic_camera_controller
{
public:
	orthographic_camera_controller(float aspect_ratio, bool rotation = false);

	void on_update(timestep ts);
	void on_event(event& event);

	void on_resize(float width, float height);
	
	float get_zoom_level() const { return m_zoom_level; }
	bool is_rotatible() const { m_rotation; }
	float get_camera_rotation() const { return m_camera_rotation; }
	float get_camera_translation_speed() const { return m_camera_translation_speed_stabil; }
	float get_camera_rotation_speed() const { return m_camera_rotation_speed; }

	const orthographic_camera_bounds& get_bounds() const { return m_bounds; }

	void set_zoom_level(float zoom_level);
	void set_rotatability(bool rotation) { m_rotation = rotation; }
	void set_camera_rotation(float rotation) { m_camera_rotation = rotation; }
	void set_camera_translation_speed(float translation_speed) { m_camera_translation_speed_stabil = translation_speed; }
	void set_camera_rotation_speed(float rotation_speed) { m_camera_rotation_speed = rotation_speed; }
	void set_camera_position(const glm::vec3& position) { m_camera_position = position;  m_camera.set_position(m_camera_position); }
	
	orthographic_camera& get_camera() { return m_camera; }
	const orthographic_camera& get_camera() const { return m_camera; }
private:
	void calculate_view();

	bool on_mouse_scrolled(mouse_scrolled_event& event);
	bool on_window_resized(window_resize_event& event);

	float m_aspect_ratio;
	float m_zoom_level = 1.0f;

	orthographic_camera_bounds m_bounds;
	orthographic_camera m_camera;

	bool m_rotation;

	glm::vec3 m_camera_position = { 0.0f, 0.0f, 0.0f };
	float m_camera_rotation = 0.0f;

	float m_camera_translation_speed_stabil = 2.0f;
	float m_camera_translation_speed = m_camera_translation_speed_stabil;
	float m_camera_rotation_speed = 120.0f;
};

_WHIP_END
