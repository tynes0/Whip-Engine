#pragma once

#include <Whip/Core/Core.h>

#include <glm/glm.hpp>

_WHIP_START

namespace Math 
{
	bool DecomposeTransform(const glm::mat4& transform, glm::vec3& translation, glm::vec3& rotation, glm::vec3& scale);
}

_WHIP_END
