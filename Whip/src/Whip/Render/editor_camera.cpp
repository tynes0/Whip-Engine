#include "whippch.h"
#include "editor_camera.h"

#include <Whip/Core/Input.h>
#include <Whip/Core/KeyCodes.h>
#include <Whip/Core/MouseButtonCodes.h>
#include <Whip/Events/KeyEvent.h>

#include <glfw/glfw3.h>

#define GLM_FORCE_QUAT_DATA_XYZW
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

_WHIP_START

editor_camera::editor_camera(float fov, float aspect_ratio, float near_clip, float far_clip) 
	: m_FOV(fov), m_aspect_ratio(aspect_ratio), m_near_clip(near_clip), m_far_clip(far_clip), 
	camera(glm::perspective(glm::radians(fov), aspect_ratio, near_clip, far_clip))
{
	update_view();
}

void editor_camera::on_update(timestep ts)
{
	if (input::is_key_down(key::left_alt))
	{
		const glm::vec2& mouse{ input::get_mouse_X(), input::get_mouse_Y() };
		glm::vec2 delta = (mouse - m_initial_mouse_position) * 0.003f;
		m_initial_mouse_position = mouse;

		if (input::is_mouse_button_down(mouse::button_right))
			mouse_pan(delta);
		else if (input::is_mouse_button_down(mouse::button_left))
			mouse_rotate(delta);
		else if (input::is_mouse_button_down(mouse::button_middle))
			mouse_zoom(delta.y);
	}

	update_view();
}

void editor_camera::on_event(event& evnt)
{
	event_dispatcher dispatcher(evnt);
	dispatcher.dispatch<mouse_scrolled_event>([this](auto&&... args) -> decltype(auto) { return this->on_mouse_scroll(std::forward<decltype(args)>(args)...); });
}

glm::vec3 editor_camera::get_up_direction() const
{
	return glm::rotate(get_orientation(), glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::vec3 editor_camera::get_right_direction() const
{
	return glm::rotate(get_orientation(), glm::vec3(1.0f, 0.0f, 0.0f));
}

glm::vec3 editor_camera::get_forward_direction() const
{
	return glm::rotate(get_orientation(), glm::vec3(0.0f, 0.0f, -1.0f));
}

glm::quat editor_camera::get_orientation() const
{
	return glm::quat(glm::vec3(-m_pitch, -m_yaw, 0.0f));
}

void editor_camera::update_projection()
{
	m_aspect_ratio = m_viewport_width / m_viewport_height;
	m_projection = glm::perspective(glm::radians(m_FOV), m_aspect_ratio, m_near_clip, m_far_clip);
}

void editor_camera::update_view()
{
	//m_yaw = m_pitch = 0.0f;
	m_position = calculate_position();

	glm::quat orientation = get_orientation();
	m_view_matrix = glm::translate(glm::mat4(1.0f), m_position) * glm::toMat4(orientation);
	m_view_matrix = glm::inverse(m_view_matrix);
}

bool editor_camera::on_mouse_scroll(mouse_scrolled_event& evnt)
{
	float delta = evnt.get_offset_y() * 0.1f;
	mouse_zoom(delta);
	update_view();
	return false;
}

void editor_camera::mouse_pan(const glm::vec2& delta)
{
	auto [xSpeed, ySpeed] = pan_speed();
	m_focal_point += -get_right_direction() * delta.x * xSpeed * m_distance;
	m_focal_point += get_up_direction() * delta.y * ySpeed * m_distance;
}

void editor_camera::mouse_rotate(const glm::vec2& delta)
{
	float yawSign = get_up_direction().y < 0 ? -1.0f : 1.0f;
	m_yaw += yawSign * delta.x * rotation_speed();
	m_pitch += delta.y * rotation_speed();
}

void editor_camera::mouse_zoom(float delta)
{
	m_distance -= delta * zoom_speed();
	if (m_distance < 1.0f)
	{
		m_focal_point += get_forward_direction();
		m_distance = 1.0f;
	}
}

glm::vec3 editor_camera::calculate_position() const
{
	return m_focal_point - get_forward_direction() * m_distance;
}

std::pair<float, float> editor_camera::pan_speed() const
{
	float x = std::min(m_viewport_width / 1000.0f, 2.4f); // max = 2.4f
	float xFactor = 0.0366f * (x * x) - 0.1778f * x + 0.3021f;

	float y = std::min(m_viewport_height / 1000.0f, 2.4f); // max = 2.4f
	float yFactor = 0.0366f * (y * y) - 0.1778f * y + 0.3021f;

	return { xFactor, yFactor };
}

float editor_camera::rotation_speed() const
{
	return 0.8f;
}

float editor_camera::zoom_speed() const
{
	float distance = m_distance * 0.2f;
	distance = std::max(distance, 0.0f);
	float speed = distance * distance;
	speed = std::min(speed, 100.0f);
	return speed;
}



_WHIP_END
