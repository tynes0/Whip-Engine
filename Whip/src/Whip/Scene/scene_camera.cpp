#include <whippch.h>
#include "scene_camera.h"

//#include <vtl/math_def.h>

#include <glm/gtc/matrix_transform.hpp>

_WHIP_START

scene_camera::scene_camera()
{
	recalculate_projection();
}

void scene_camera::set_perspective(float vertical_FOV, float near_clip, float far_clip)
{
	m_projection_type = projection_type::perspective;
	m_perspective_FOV = vertical_FOV;
	m_perspective_near = near_clip;
	m_perspective_far = far_clip;
	recalculate_projection();
}

void scene_camera::set_orthographic(float size, float near_clip, float far_clip)
{
	m_orthographic_size = size;
	m_orthographic_near = near_clip;
	m_orthographic_far = far_clip;
	recalculate_projection();
}

void scene_camera::set_viewport_size(uint32_t width, uint32_t height)
{
	WHP_CORE_ASSERT(width > 0 && height > 0);
	m_aspect_ratio = ((float)width/ (float)height);
	recalculate_projection();
}

void scene_camera::set_perspective_vertical_FOV(float vertical_FOV)
{
	m_perspective_FOV = vertical_FOV;
	recalculate_projection();
}

void scene_camera::set_perspective_near_clip(float near_clip)
{
	m_perspective_near = near_clip;
	recalculate_projection();
}

void scene_camera::set_perspective_far_clip(float far_clip)
{
	m_perspective_far = far_clip;
	recalculate_projection();
}

void scene_camera::set_orthographic_size(float size)
{
	m_orthographic_size = size;
	recalculate_projection();
}

void scene_camera::set_orthographic_near_clip(float near_clip)
{
	m_orthographic_near = near_clip;
	recalculate_projection();
}

void scene_camera::set_orthographic_far_clip(float far_clip)
{
	m_orthographic_far = far_clip;
	recalculate_projection();
}

void scene_camera::set_projection_type(projection_type type)
{
	m_projection_type = type;
	recalculate_projection();
}

void scene_camera::recalculate_projection()
{
	if (m_projection_type == projection_type::perspective)
	{
		m_projection = glm::perspective(m_perspective_FOV, m_aspect_ratio, m_perspective_near, m_perspective_far);
	}
	else
	{
		float ortho_left = -m_orthographic_size * m_aspect_ratio * 0.5f;
		float ortho_right = m_orthographic_size * m_aspect_ratio * 0.5f;
		float ortho_bottom = -m_orthographic_size * 0.5f;
		float ortho_top = m_orthographic_size * 0.5f;

		m_projection = glm::ortho(ortho_left, ortho_right, ortho_bottom, ortho_top, m_orthographic_near, m_orthographic_far);
	}
}

_WHIP_END
