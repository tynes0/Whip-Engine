#include "whippch.h"
#include "orthographic_camera.h"

#include <glm/gtc/matrix_transform.hpp>

_WHIP_START

orthographic_camera::orthographic_camera(float left, float right, float bottom, float top) : m_projection_matrix(glm::ortho(left, right, bottom, top, -1.0f, 1.0f)),  m_view_matrix(1.0f)
{
	WHP_PROFILE_FUNCTION();

	m_view_projection_matrix = m_projection_matrix * m_view_matrix;
}

void orthographic_camera::recalculate_view_matrix()
{
	WHP_PROFILE_FUNCTION();

	glm::mat4 transform = glm::translate(glm::mat4(1.0f), m_position) * glm::rotate(glm::mat4(1.0f), glm::radians(m_rotation), glm::vec3(0, 0, 1));

	m_view_matrix = glm::inverse(transform);
	m_view_projection_matrix = m_projection_matrix * m_view_matrix;
}

void orthographic_camera::set_projection(float left, float right, float bottom, float top)
{
	WHP_PROFILE_FUNCTION();

	m_projection_matrix = glm::ortho(left, right, bottom, top, -1.0f, 1.0f);
	m_view_projection_matrix = m_projection_matrix * m_view_matrix;
}

void orthographic_camera::set_position(const glm::vec3& position)
{ 
	m_position = position;
	recalculate_view_matrix();
}

void orthographic_camera::set_rotation(float rotation)
{
	m_rotation = rotation;
	recalculate_view_matrix();
}


_WHIP_END


