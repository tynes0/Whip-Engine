#include "whippch.h"
#include "OrthographicCameraController.h"


_WHIP_START

orthographic_camera_controller::orthographic_camera_controller(float aspect_ratio, bool rotation)
	:m_aspect_ratio(aspect_ratio), m_camera(-m_aspect_ratio * m_zoom_level, m_aspect_ratio * m_zoom_level, -m_zoom_level, m_zoom_level), m_rotation(rotation) {}

void orthographic_camera_controller::on_update(timestep ts)
{
	WHP_PROFILE_FUNCTION();

	if (input::is_key_pressed(buttons.camera_to_left))
	{
		m_camera_position.x -= cos(glm::radians(m_camera_rotation)) * m_camera_translation_speed * ts;
		m_camera_position.y -= sin(glm::radians(m_camera_rotation)) * m_camera_translation_speed * ts;
	}
	else if (input::is_key_pressed(buttons.camera_to_right))
	{
		m_camera_position.x += cos(glm::radians(m_camera_rotation)) * m_camera_translation_speed * ts;
		m_camera_position.y += sin(glm::radians(m_camera_rotation)) * m_camera_translation_speed * ts;
	}
	if (input::is_key_pressed(buttons.camera_to_down))
	{
		m_camera_position.x -= -sin(glm::radians(m_camera_rotation)) * m_camera_translation_speed * ts;
		m_camera_position.y -= cos(glm::radians(m_camera_rotation)) * m_camera_translation_speed * ts;
	}
	else if (input::is_key_pressed(buttons.camera_to_up))
	{
		m_camera_position.x += -sin(glm::radians(m_camera_rotation)) * m_camera_translation_speed * ts;
		m_camera_position.y += cos(glm::radians(m_camera_rotation)) * m_camera_translation_speed * ts;
	}
	if(m_rotation)
	{
		if (input::is_key_pressed(buttons.camera_rotation_left))
		{
			m_camera_rotation += m_camera_rotation_speed * ts;
		}
		else if (input::is_key_pressed(buttons.camera_rotation_right))
		{
			m_camera_rotation -= m_camera_rotation_speed * ts;
		}
		if (m_camera_rotation > 180.0f)
		{
			m_camera_rotation -= 360.0f;
		}
		else if (m_camera_rotation <= -180.0f)
		{
			m_camera_rotation += 360.0f;
		}
		m_camera.set_rotation(m_camera_rotation);
	}
	m_camera.set_position(m_camera_position);
	set_camera_translation_speed(m_zoom_level * 2.0f);
}

void orthographic_camera_controller::on_event(Event& event)
{
	WHP_PROFILE_FUNCTION();

	event_dispatcher dispatcher(event);
	dispatcher.dispatch<mouse_scrolled_event>(WHP_BIND_EVENT_FN(orthographic_camera_controller::on_mouse_scrolled));
	dispatcher.dispatch<window_resize_event>(WHP_BIND_EVENT_FN(orthographic_camera_controller::on_window_resized));
}

bool orthographic_camera_controller::on_mouse_scrolled(mouse_scrolled_event& event)
{
	WHP_PROFILE_FUNCTION();

	m_zoom_level -= event.get_offset_y() * 0.15f;
	m_zoom_level = (m_zoom_level > 0.1f) ? m_zoom_level : 0.1f;
	m_camera.set_projection(-m_aspect_ratio * m_zoom_level, m_aspect_ratio * m_zoom_level, -m_zoom_level, m_zoom_level);
	return false;
}

bool orthographic_camera_controller::on_window_resized(window_resize_event& event)
{
	WHP_PROFILE_FUNCTION();

	m_aspect_ratio = calculate_aspect_ratio((float)event.get_width(), (float)event.get_height());
	m_camera.set_projection(-m_aspect_ratio * m_zoom_level, m_aspect_ratio * m_zoom_level, -m_zoom_level, m_zoom_level);
	return false;
}

_WHIP_END