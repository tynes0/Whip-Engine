#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Render/camera.h>

_WHIP_START

class scene_camera : public camera
{
public:
	enum class projection_type { perspective = 0, orthographic = 1 };
public:
	scene_camera();
	virtual ~scene_camera() = default;

	void set_viewport_size(uint32_t width, uint32_t height);

	void set_perspective(float vertical_FOV, float near_clip, float far_clip);
	void set_orthographic(float size, float near_clip, float far_clip);

	float get_perspective_vertical_FOV() const { return m_perspective_FOV; }
	float get_perspective_near_clip() const { return m_perspective_near; }	
	float get_perspective_far_clip() const { return m_perspective_far; }	
	float get_orthographic_size() const { return m_orthographic_size; }
	float get_orthographic_near_clip() const { return m_orthographic_near; }
	float get_orthographic_far_clip() const { return m_orthographic_far; }

	void set_perspective_vertical_FOV(float vertical_FOV);
	void set_perspective_near_clip(float near_clip);
	void set_perspective_far_clip(float far_clip);
	void set_orthographic_size(float size);
	void set_orthographic_near_clip(float near_clip);
	void set_orthographic_far_clip(float far_clip);

	projection_type get_projection_type() const { return m_projection_type; }
	void set_projection_type(projection_type type);
private:
	void recalculate_projection();
private:
	projection_type m_projection_type = projection_type::orthographic;
	float m_perspective_FOV = glm::radians(45.0f);
	float m_perspective_near = 0.01f;
	float m_perspective_far = 1000.0f;
	float m_orthographic_size = 10.0f;
	float m_orthographic_near = -1.0f;
	float m_orthographic_far = 1.0f;
	float m_aspect_ratio = 0.0f;
};

_WHIP_END