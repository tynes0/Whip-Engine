#pragma once

#include <Whip/Core/Core.h>

#include <glm/glm.hpp>

_WHIP_START

class orthographic_camera
{
private:
	glm::mat4 m_projection_matrix;
	glm::mat4 m_view_matrix;
	glm::mat4 m_view_projection_matrix;
	glm::vec3 m_position = { 0.0f, 0.0f, 0.0f };
	float m_rotation = 0.0f;
private:
	void recalculate_view_matrix();
public:
	orthographic_camera(float left, float right, float bottom, float top);

	void set_projection(float left, float right, float bottom, float top);

	void set_position(const glm::vec3& position);
	void set_rotation(float rotation);

	WHP_NODISCARD const glm::vec3& get_position() const { return m_position; }
	WHP_NODISCARD float get_rotation() const { return m_rotation; }

	WHP_NODISCARD const glm::mat4& get_projection_matrix() const { return m_projection_matrix; }
	WHP_NODISCARD const glm::mat4& get_view_matrix() const { return m_view_matrix; }
	WHP_NODISCARD const glm::mat4& get_view_projection_matrix() const { return m_view_projection_matrix; }
};

_WHIP_END