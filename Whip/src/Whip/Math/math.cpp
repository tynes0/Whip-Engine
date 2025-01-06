#include "whippch.h"
#include "math.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

_WHIP_START

namespace math
{
	bool decompose_transform(const glm::mat4& transform, glm::vec3& translation, glm::vec3& rotation, glm::vec3& scale)
	{
		using namespace glm;

		mat4 local_matrix(transform);

		if (epsilonEqual(local_matrix[3][3], 0.0f, epsilon<float>()))
			return false;

		if (epsilonNotEqual(local_matrix[0][3], 0.0f, epsilon<float>()) ||
			epsilonNotEqual(local_matrix[1][3], 0.0f, epsilon<float>()) ||
			epsilonNotEqual(local_matrix[2][3], 0.0f, epsilon<float>()))
		{
			local_matrix[0][3] = local_matrix[1][3] = local_matrix[2][3] = 0.0f;
			local_matrix[3][3] = 1.0f;
		}

		translation = vec3(local_matrix[3]);
		local_matrix[3] = vec4(0, 0, 0, local_matrix[3].w);

		vec3 row[3];

		for (length_t i = 0; i < 3; ++i)
			for (length_t j = 0; j < 3; ++j)
				row[i][j] = local_matrix[i][j];

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

		quat rotation_quat = quat_cast(mat3(row[0], row[1], row[2]));

		rotation = eulerAngles(rotation_quat);

		return true;
	}
}
_WHIP_END
