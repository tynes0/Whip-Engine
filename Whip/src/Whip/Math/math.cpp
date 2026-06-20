#include "WhipPch.h"
#include <Whip/Math/Math.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

_WHIP_START

namespace Math
{
	bool DecomposeTransform(const glm::mat4& transform, glm::vec3& translation, glm::vec3& rotation, glm::vec3& scale)
	{
		using namespace glm;

		mat4 localMatrix(transform);

		if (epsilonEqual(localMatrix[3][3], 0.0f, epsilon<float>()))
			return false;

		if (epsilonNotEqual(localMatrix[0][3], 0.0f, epsilon<float>()) ||
			epsilonNotEqual(localMatrix[1][3], 0.0f, epsilon<float>()) ||
			epsilonNotEqual(localMatrix[2][3], 0.0f, epsilon<float>()))
		{
			localMatrix[0][3] = localMatrix[1][3] = localMatrix[2][3] = 0.0f;
			localMatrix[3][3] = 1.0f;
		}

		translation = vec3(localMatrix[3]);
		localMatrix[3] = vec4(0, 0, 0, localMatrix[3].w);

		vec3 row[3];

		for (length_t i = 0; i < 3; ++i)
			for (length_t j = 0; j < 3; ++j)
				row[i][j] = localMatrix[i][j];

		scale.x = length(row[0]);
		if (epsilonEqual(scale.x, 0.0f, epsilon<float>()))
			return false;
		row[0] = row[0] / scale.x;

		scale.y = length(row[1]);
		if (epsilonEqual(scale.y, 0.0f, epsilon<float>()))
			return false;
		row[1] = row[1] / scale.y;

		scale.z = length(row[2]);
		if (epsilonEqual(scale.z, 0.0f, epsilon<float>()))
			return false;
		row[2] = row[2] / scale.z;

		quat rotationQuat = quat_cast(mat3(row[0], row[1], row[2]));

		rotation = eulerAngles(rotationQuat);

		return true;
	}

	bool EqualF(float a, float b, float epsilon)
	{
		return std::abs(a - b) <= epsilon;
	}

	bool EqualF(double a, double b, double epsilon)
	{
		return std::abs(a - b) <= epsilon;
	}
}
_WHIP_END
