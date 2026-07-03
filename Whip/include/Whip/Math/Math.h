#pragma once

#include <Whip/Core/Core.h>

#include <glm/glm.hpp>

_WHIP_START

namespace Math
{
	bool DecomposeTransform(const glm::mat4& transform, glm::vec3& translation, glm::vec3& rotation, glm::vec3& scale);
	[[nodiscard]] bool EqualF(float a, float b, float epsilon = std::numeric_limits<float>::epsilon());
	[[nodiscard]] bool EqualF(double a, double b, double epsilon = std::numeric_limits<double>::epsilon());
}

_WHIP_END
