#pragma once

#include <Whip/Core/Core.h>

#include "camera.h"
#include <Whip/Core/Timestep.h>
#include <Whip/Events/Event.h>
#include <Whip/Events/MouseEvent.h>

#include <glm/glm.hpp>

_WHIP_START

class editor_camera : public camera
{
public:
	editor_camera() = default;
	editor_camera(float fov, float aspect_ratio, float near_clip, float far_clip);

	void on_update(timestep ts);
	void on_event(event& evnt);

	inline float get_distance() const { return m_distance; }
	inline void set_distance(float distance) { m_distance = distance; }

	inline void set_viewport_size(float width, float height) { m_viewport_width = width; m_viewport_height = height; update_projection(); }

	const glm::mat4& get_view_matrix() const { return m_view_matrix; }
	glm::mat4 get_view_projection() const { return m_projection * m_view_matrix; }

	glm::vec3 get_up_direction() const;
	glm::vec3 get_right_direction() const;
	glm::vec3 get_forward_direction() const;
	const glm::vec3& get_position() const { return m_position; }
	glm::quat get_orientation() const;

	float get_pitch() const { return m_pitch; }
	float get_yaw() const { return m_yaw; }
private:
	void update_projection();
	void update_view();

	bool on_mouse_scroll(mouse_scrolled_event& evnt);

	void mouse_pan(const glm::vec2& delta);
	void mouse_rotate(const glm::vec2& delta);
	void mouse_zoom(float delta);

	glm::vec3 calculate_position() const;

	std::pair<float, float> pan_speed() const;
	float rotation_speed() const;
	float zoom_speed() const;
private:
	float m_FOV = 45.0f, m_aspect_ratio = 1.778f, m_near_clip = 0.1f, m_far_clip = 1000.0f;

	glm::mat4 m_view_matrix;
	glm::vec3 m_position = { 0.0f, 0.0f, 0.0f };
	glm::vec3 m_focal_point = { 0.0f, 0.0f, 0.0f };

	glm::vec2 m_initial_mouse_position = { 0.0f, 0.0f };

	float m_distance = 10.0f;
	float m_pitch = 0.0f, m_yaw = 0.0f;

	float m_viewport_width = 1280, m_viewport_height = 720;
};

_WHIP_END
