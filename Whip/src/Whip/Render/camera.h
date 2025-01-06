#pragma once

#include <Whip/Core/Core.h>

#include <glm/glm.hpp>


_WHIP_START

class camera
{
public:
	camera() = default;
	camera(const glm::mat4& projection) : m_projection(projection) {}

	virtual ~camera() = default;

	const glm::mat4& get_projection() const { return m_projection; }
protected:
	glm::mat4 m_projection = glm::mat4(1.0f);
};

_WHIP_END