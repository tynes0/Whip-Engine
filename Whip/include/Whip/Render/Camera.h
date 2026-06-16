#pragma once

#include <Whip/Core/Core.h>

#include <glm/glm.hpp>

_WHIP_START

class Camera
{
public:
	Camera() = default;
	Camera(const glm::mat4& projection) : m_Projection(projection) {}

	virtual ~Camera() = default;

	const glm::mat4& GetProjection() const { return m_Projection; }
protected:
	glm::mat4 m_Projection = glm::mat4(1.0f);
};

_WHIP_END
